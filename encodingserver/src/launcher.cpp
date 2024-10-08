#include "launcher.h"
#include <unistd.h>
#include <string>
#include "log.h"
#include <thread>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vector>
#include <sstream>
#include <string.h>
#include "aes67utils.h"
#include "jsonconsts.h"

using namespace std::placeholders;

Launcher::Launcher(asio::io_context& context, std::string_view sEncoderApp, const std::string& sRecorder) :
    m_context(context),
    m_timer(context),
    m_sEncoderApp(sEncoderApp),
    m_sRecorder(sRecorder)
{
    
}

Launcher::~Launcher() = default;

void Launcher::SetPaths(const std::string& sWav, const std::string& sEncoded, const std::filesystem::path& socket)
{
    m_sPathWav = sWav;
    m_sPathEncoded = sEncoded;
    
    m_pathSocket = socket;
    m_sPathSocket = m_pathSocket.string();

    m_jsStatus["id"] = m_pathSocket.stem().string();
}


void Launcher::SetCallbacks(const std::function<void(const std::string&, const Json::Value&)>& statusCallback, const std::function<void(const std::string&, int, bool)>& exitCallback)
{
    m_statusCallback = statusCallback;
    m_exitCallback = exitCallback;
}

bool Launcher::IsRunning() const
{
    return (m_pid!=0);
}

int Launcher::LaunchEncoder()
{
    pmlLog(pml::LOG_DEBUG, m_sRecorder) << "LaunchEncoder: " << m_pathSocket.stem().string();
    if(CheckForOrphanedEncoder())
    {
        return m_pid;
    }
    else
    {
        m_pid = fork();
        if(m_pid < 0)
        {
            pmlLog(pml::LOG_ERROR, m_sRecorder) << "could not fork: " << strerror(errno) << std::endl;

            return FORK_ERROR;
        }
        else if(m_pid > 0)
        {   // Parent
            pmlLog(pml::LOG_INFO, m_sRecorder) << "Encoder launched";
            m_pathSocket.replace_extension(std::to_string(m_pid));
            m_pSocket = std::make_shared<asio::local::stream_protocol::socket>(m_context);
            Connect(std::chrono::milliseconds(50));
            return m_pid;
        }
        else
        {   //child

            //create the args and launch the Encoder
            //name, audio path, socket path, logging
            m_sPathSocket = m_pathSocket.string();
            char* args[] = {&m_sEncoderApp[0], &m_sRecorder[0], &m_sPathWav[0], &m_sPathEncoded[0], &m_sPathSocket[0], nullptr};
            if(execv(m_sEncoderApp.c_str(), args) != 0)
            {
                std::cout << "Exec failed: " << m_sEncoderApp << std::endl;
                exit(-1);
            }
            return 0;
        }
    }
}


void Launcher::Connect(const std::chrono::milliseconds& wait)
{
    try
    {
        pmlLog(pml::LOG_INFO, m_sRecorder) << "Start timer to connect to " << m_pathSocket.stem().string();
        if(m_context.stopped())
        {
            pmlLog(pml::LOG_ERROR, m_sRecorder) << "Context has stopped!!";
        }
        m_timer.expires_from_now(wait);
        m_timer.async_wait([this](const asio::error_code& e)
        {
            if(!e && m_pSocket)
            {
                pmlLog(pml::LOG_INFO, m_sRecorder) << "Try to connect to " << m_pathSocket.stem().string();
                m_pSocket->async_connect(asio::local::stream_protocol::endpoint(m_pathSocket.string()), 
                std::bind(&Launcher::HandleConnect, this, _1));
            }
            else if(e != asio::error::operation_aborted)
            {
                pmlLog(pml::LOG_ERROR, m_sRecorder) << "deadline timer failed!";
            }
            else
            {
                pmlLog(pml::LOG_DEBUG, m_sRecorder) << "deadline aborted";
            }
         });
    }
    catch(asio::system_error& e)
    {
        pmlLog(pml::LOG_ERROR, m_sRecorder) << "Could not connect create timer to connect to Encoder!";
    }
}


bool Launcher::RestartEncoder()
{
    pmlLog(pml::LOG_INFO, m_sRecorder) << m_pathSocket.stem().string() << " restart Encoder";
    m_bMarkedForRemoval = false;
    CloseAndLaunchOrRemove(SIGKILL);
    return true;
}

bool Launcher::RemoveEncoder()
{
    pmlLog(pml::LOG_INFO, m_sRecorder) << m_pathSocket.stem().string() << " remove Encoder";
    m_bMarkedForRemoval = true;
    CloseAndLaunchOrRemove(SIGKILL);
    return true;
}


int Launcher::CloseEncoder(int nSignal)
{
    if(m_pid != 0)
    {
        pmlLog(pml::LOG_INFO, m_sRecorder) << m_pathSocket.stem().string() << " stop read socket timer";
        m_timer.cancel();

        pmlLog(pml::LOG_INFO, m_sRecorder) << m_pathSocket.stem().string() << " close the socket";
        m_pSocket->close();

        m_pSocket = nullptr;

        pmlLog(pml::LOG_INFO, m_sRecorder) << m_pathSocket.stem().string() << " send " << nSignal << " to pid " << m_pid;
        kill(m_pid, nSignal);
        int nStatus;

        pmlLog(pml::LOG_INFO, m_sRecorder) << m_pathSocket.stem().string() << " waitpid";
        waitpid(m_pid, &nStatus,0);

        pmlLog(pml::LOG_INFO, m_sRecorder) << m_pathSocket.stem().string() << " waitpid returned " << nStatus << " now remove socket file it still there";

        if(m_pathSocket.has_extension())
        {
            try
            {
                std::filesystem::remove(m_pathSocket);
                m_pathSocket.replace_extension();
            }
            catch(std::filesystem::filesystem_error& e)
            {
                pmlLog(pml::LOG_WARN, m_sRecorder) << "Could not remove socket " << e.what();
            }
        }
        return nStatus;
    }
    return 0;
}

void Launcher::CloseAndLaunchOrRemove(int nSignal)
{
    int nStatus = CloseEncoder(nSignal);

    if(!m_bMarkedForRemoval)
    {
        m_exitCallback(m_pathSocket.stem().string(), nStatus, false);
        LaunchEncoder();
    }
    else
    {
        DoRemoveEncoder();
        m_exitCallback(m_pathSocket.stem().string(), nStatus, true);
    }
}

void Launcher::Read()
{
    m_timer.cancel();
    m_timer.expires_from_now(std::chrono::seconds(10));         //@todo this should be double the heartbeat rate or something
    m_timer.async_wait([this](const asio::error_code& e)
    {
        if(!e)
        {
            pmlLog(pml::LOG_ERROR, m_sRecorder) << m_pathSocket << "\tTimeout - close Encoder and relaunch" << std::endl;
            CloseAndLaunchOrRemove(SIGKILL);
        }
        else if(e != asio::error::operation_aborted)
        {
            pmlLog(pml::LOG_ERROR, m_sRecorder) << "deadline timer failed!";
        }
    });

    if(m_pSocket)
    {
        m_pSocket->async_read_some(asio::buffer(m_data), std::bind(&Launcher::HandleRead, this, _1, _2));
    }
}

void Launcher::HandleRead(std::error_code ec, std::size_t nLength)
{
    m_timer.cancel();

    if (!ec)
    {
        auto vOut = ExtractReadBuffer(nLength);

        for(const auto& sLine : vOut)
        {
            auto js = ConvertToJson(sLine);
            if(js)
            {
                m_jsStatus = *js;
                CreateSummary();
            }
        }
        if(m_statusCallback)
        {
            m_statusCallback(m_pathSocket.stem().string(), m_jsStatus);
        }

        Read();
    }
    else if(ec != asio::error::operation_aborted)
    {
        pmlLog(pml::LOG_ERROR, m_sRecorder) << m_pathSocket << "\tEOF or Read Error: " << ec.message();
        CloseAndLaunchOrRemove(SIGKILL);
    }
}
std::vector<std::string> Launcher::ExtractReadBuffer(size_t nLength)
{
    m_sOut.append(m_data.begin(), m_data.begin()+nLength);

    std::vector<std::string> vLines;

    //do we have at least \n
    if(size_t nLastLineBreak = m_sOut.find_last_of('\n'); nLastLineBreak != std::string::npos)
    {
        std::string sComplete = m_sOut.substr(0, nLastLineBreak);
        vLines = SplitString(sComplete, '\n');

        if(nLastLineBreak != m_sOut.size()-1)
        {
            m_sOut = m_sOut.substr(nLastLineBreak+1);
        }
        else
        {
            m_sOut.clear();
        }
    }
    return vLines;
}


void Launcher::HandleConnect(const asio::error_code& e)
{
    if(!e)
    {
        pmlLog(pml::LOG_INFO, m_sRecorder) << "Connected to " << m_pathSocket.stem().string();
        Read();
    }
    else
    {
        pmlLog(pml::LOG_ERROR, m_sRecorder) << "Could not connect to Encoder " << m_pathSocket.stem().string() << " " << e.message() << " " << m_pathSocket;
        Connect(std::chrono::seconds(1));
    }
}


void Launcher::CreateSummary()
{
    m_jsStatusSummary = m_jsStatus;

    
}
const Json::Value& Launcher::GetStatusSummary() const
{
    return m_jsStatusSummary;
}

bool Launcher::CheckForOrphanedEncoder()
{
    auto pathOrphan = m_pathSocket.parent_path();

    pmlLog(pml::LOG_INFO, m_sRecorder) << "CheckForOrphanedEncoder in " << pathOrphan;

    for(const auto& entry : std::filesystem::directory_iterator(pathOrphan))
    {
        if(entry.path().stem() == m_pathSocket.stem())  //this is a socket with the same name as our Encoder
        {
            int nPid;
            try
            {
                nPid = std::stoi(entry.path().extension().string().substr(1));
            }
            catch(...)
            {
                nPid  = -1;
            }

            if(nPid > 0)
            {
                if(kill(nPid, 0) == 0)
                {
                    pmlLog(pml::LOG_INFO, m_sRecorder) << "Encoder " << m_pathSocket.stem().string() << " is still running on pid " << nPid;
                    m_pid = nPid;
                    m_pathSocket.replace_extension(std::to_string(m_pid));
                    m_pSocket = std::make_shared<asio::local::stream_protocol::socket>(m_context);
                    Connect(std::chrono::milliseconds(5));
                    return true;
                }
                else
                {
                    pmlLog(pml::LOG_INFO, m_sRecorder) << "Encoder " << m_pathSocket.stem().string() << " was running on pid " << nPid << " and has left socket open. Close it";
                    try
                    {
                        std::filesystem::remove(entry.path());
                    }
                    catch(std::filesystem::filesystem_error& e)
                    {
                        pmlLog(pml::LOG_WARN, m_sRecorder) << "Could not remove socket " << e.what();
                    }
                }
            }
        }
    }
    return false;
}

void Launcher::DoRemoveEncoder()
{
    try
    {
        std::filesystem::remove(m_pathSocket);
        pmlLog(pml::LOG_INFO, m_sRecorder) << "Sockets for " << m_pathSocket.stem().string() << " removed";
    }
    catch(std::filesystem::filesystem_error& e)
    {
        pmlLog(pml::LOG_INFO, m_sRecorder) << m_pathSocket.stem().string() << " failed to remove all files " << e.what();
    }
}
