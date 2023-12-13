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

Launcher::Launcher(asio::io_context& context, const std::filesystem::path& pathConfig, const std::filesystem::path& pathSocket,
const std::function<void(const std::string&, const Json::Value&)>& statusCallback, 
const std::function<void(const std::string&, int, bool)>& exitCallback) :
    m_context(context),
    m_timer(context),
    m_pathConfig(pathConfig),
    m_pathSocket(pathSocket),
    m_statusCallback(statusCallback),
    m_exitCallback(exitCallback)
{
    m_jsStatus["id"] = m_pathConfig.stem().string();
}

Launcher::~Launcher() = default;


bool Launcher::IsRunning() const
{
    return (m_pid!=0);
}

int Launcher::LaunchLogger()
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "LaunchLogger: " << m_pathConfig.stem().string();
    if(CheckForOrphanedLogger())
    {
        return m_pid;
    }
    else
    {
        m_pid = fork();
        if(m_pid < 0)
        {
            pmlLog(pml::LOG_ERROR, "aes67") << "could not fork: " << strerror(errno) << std::endl;

            return FORK_ERROR;
        }
        else if(m_pid > 0)
        {   // Parent
            pmlLog(pml::LOG_INFO, "aes67") << "Logger launched";
            m_pathSocket.replace_extension(std::to_string(m_pid));
            m_pSocket = std::make_shared<asio::local::stream_protocol::socket>(m_context);
            Connect(std::chrono::milliseconds(50));
            return m_pid;
        }
        else
        {   //child

            //create the args and launch the logger
            m_sConfigArg = m_pathConfig.string();
            char* args[] = {&m_sLoggerApp[0], &m_sConfigArg[0], nullptr};

            if(execv(m_sLoggerApp.c_str(), args) != 0)
            {
                std::cout << "Exec failed: " << m_sLoggerApp << std::endl;
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
        pmlLog(pml::LOG_INFO, "aes67") << "Start timer to connect to " << m_pathConfig.stem().string();
        if(m_context.stopped())
        {
            pmlLog(pml::LOG_ERROR, "aes67") << "Context has stopped!!";
        }
        m_timer.expires_from_now(wait);
        m_timer.async_wait([this](const asio::error_code& e)
        {
            if(!e && m_pSocket)
            {
                pmlLog(pml::LOG_INFO, "aes67") << "Try to connect to " << m_pathConfig.stem().string();
                m_pSocket->async_connect(asio::local::stream_protocol::endpoint(m_pathSocket.string()), 
                std::bind(&Launcher::HandleConnect, this, _1));
            }
            else if(e != asio::error::operation_aborted)
            {
                pmlLog(pml::LOG_ERROR, "aes67") << "deadline timer failed!";
            }
            else
            {
                pmlLog(pml::LOG_DEBUG, "aes67") << "deadline aborted";
            }
         });
    }
    catch(asio::system_error& e)
    {
        pmlLog(pml::LOG_ERROR, "aes67") << "Could not connect create timer to connect to logger!";
    }
}


bool Launcher::RestartLogger()
{
    pmlLog(pml::LOG_INFO, "aes67") << m_pathConfig.stem().string() << " restart logger";
    m_bMarkedForRemoval = false;
    CloseAndLaunchOrRemove(SIGKILL);
    return true;
}

bool Launcher::RemoveLogger()
{
    pmlLog(pml::LOG_INFO, "aes67") << m_pathConfig.stem().string() << " remove logger";
    m_bMarkedForRemoval = true;
    CloseAndLaunchOrRemove(SIGKILL);
    return true;
}


int Launcher::CloseLogger(int nSignal)
{
    if(m_pid != 0)
    {
        pmlLog(pml::LOG_INFO, "aes67") << m_pathConfig.stem().string() << " stop read socket timer";
        m_timer.cancel();

        pmlLog(pml::LOG_INFO, "aes67") << m_pathConfig.stem().string() << " close the socket";
        m_pSocket->close();

        m_pSocket = nullptr;

        pmlLog(pml::LOG_INFO, "aes67") << m_pathConfig.stem().string() << " send " << nSignal << " to pid " << m_pid;
        kill(m_pid, nSignal);
        int nStatus;

        pmlLog(pml::LOG_INFO, "aes67") << m_pathConfig.stem().string() << " waitpid";
        waitpid(m_pid, &nStatus,0);

        pmlLog(pml::LOG_INFO, "aes67") << m_pathConfig.stem().string() << " waitpid returned " << nStatus << " now remove socket file it still there";

        if(m_pathSocket.has_extension())
        {
            try
            {
                std::filesystem::remove(m_pathSocket);
                m_pathSocket.replace_extension();
            }
            catch(std::filesystem::filesystem_error& e)
            {
                pmlLog(pml::LOG_WARN, "aes67") << "Could not remove socket " << e.what();
            }
        }
        return nStatus;
    }
    return 0;
}

void Launcher::CloseAndLaunchOrRemove(int nSignal)
{
    int nStatus = CloseLogger(nSignal);

    if(!m_bMarkedForRemoval)
    {
        m_exitCallback(m_pathConfig.stem().string(), nStatus, false);
        LaunchLogger();
    }
    else
    {
        DoRemoveLogger();
        m_exitCallback(m_pathConfig.stem().string(), nStatus, true);
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
            pmlLog(pml::LOG_ERROR, "aes67") << m_pathConfig << "\tTimeout - close logger and relaunch" << std::endl;
            CloseAndLaunchOrRemove(SIGKILL);
        }
        else if(e != asio::error::operation_aborted)
        {
            pmlLog(pml::LOG_ERROR, "aes67") << "deadline timer failed!";
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
            m_statusCallback(m_pathConfig.stem().string(), m_jsStatus);
        }

        Read();
    }
    else if(ec != asio::error::operation_aborted)
    {
        pmlLog(pml::LOG_ERROR, "aes67") << m_pathConfig << "\tEOF or Read Error: " << ec.message();
        CloseAndLaunchOrRemove(SIGKILL);
    }
}
std::vector<std::string> Launcher::ExtractReadBuffer(size_t nLength)
{
    //std::lock_guard<std::mutex> lg(m_mutex);

    m_sOut.append(m_data.begin(), m_data.begin()+nLength);

    std::vector<std::string> vLines;

    //do we have at least \n
    size_t nLastLineBreak = m_sOut.find_last_of('\n');
    if(nLastLineBreak != std::string::npos)
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
        pmlLog(pml::LOG_INFO, "aes67") << "Connected to " << m_pathConfig.stem().string();
        Read();
    }
    else
    {
        pmlLog(pml::LOG_ERROR, "aes67") << "Could not connect to logger " << m_pathConfig.stem().string() << " " << e.message() << " " << m_pathSocket;
        Connect(std::chrono::seconds(1));
    }
}


void Launcher::CreateSummary()
{
    std::scoped_lock lg(m_mutex);
    m_jsStatusSummary.clear();
    m_jsStatusSummary[jsonConsts::name] = m_pathConfig.stem().string();
    m_jsStatusSummary[jsonConsts::running] = IsRunning();

    if(m_jsStatus.isMember(jsonConsts::streaming))
    {
        m_jsStatusSummary[jsonConsts::streaming] = m_jsStatus[jsonConsts::streaming];
    }

    if(m_jsStatus.isMember(jsonConsts::heartbeat))
    {
        m_jsStatusSummary[jsonConsts::timestamp] = m_jsStatus[jsonConsts::heartbeat][jsonConsts::timestamp];
        m_jsStatusSummary[jsonConsts::up_time] = m_jsStatus[jsonConsts::heartbeat][jsonConsts::up_time];
    }

    if(m_jsStatus.isMember(jsonConsts::file) && m_jsStatus[jsonConsts::file].isMember(jsonConsts::filename))
    {
        m_jsStatusSummary[jsonConsts::filename] = m_jsStatus[jsonConsts::file][jsonConsts::filename];
    }

}
Json::Value Launcher::GetStatusSummary() const
{
    std::scoped_lock lg(m_mutex);
    return m_jsStatusSummary;
}

bool Launcher::CheckForOrphanedLogger()
{
    auto pathOrphan = m_pathSocket.parent_path();

    pmlLog(pml::LOG_INFO, "aes67") << "CheckForOrphanedLogger in " << pathOrphan;

    for(const auto& entry : std::filesystem::directory_iterator(pathOrphan))
    {
        if(entry.path().stem() == m_pathConfig.stem())  //this is a socket with the same name as our logger
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
                    pmlLog(pml::LOG_INFO, "aes67") << "Logger " << m_pathConfig.stem().string() << " is still running on pid " << nPid;
                    m_pid = nPid;
                    m_pathSocket.replace_extension(std::to_string(m_pid));
                    m_pSocket = std::make_shared<asio::local::stream_protocol::socket>(m_context);
                    Connect(std::chrono::milliseconds(5));
                    return true;
                }
                else
                {
                    pmlLog(pml::LOG_INFO, "aes67") << "Logger " << m_pathConfig.stem().string() << " was running on pid " << nPid << " and has left socket open. Close it";
                    try
                    {
                        std::filesystem::remove(entry.path());
                    }
                    catch(std::filesystem::filesystem_error& e)
                    {
                        pmlLog(pml::LOG_WARN, "aes67") << "Could not remove socket " << e.what();
                    }
                }
            }
        }
    }
    return false;
}

void Launcher::DoRemoveLogger()
{
    try
    {
        std::filesystem::remove(m_pathConfig);
        std::filesystem::remove(m_pathSocket);
        pmlLog(pml::LOG_INFO, "aes67") << "config and sockets for " << m_pathConfig.stem().string() << " removed";
    }
    catch(std::filesystem::filesystem_error& e)
    {
        pmlLog(pml::LOG_INFO, "aes67") << m_pathConfig.stem().string() << " failed to remove all files " << e.what();
    }
}
