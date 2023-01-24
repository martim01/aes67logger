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
#include "aoiputils.h"
#include "jsonconsts.h"


bool ReadFromPipe(int nFd, std::string& sBuffer, std::vector<std::string>& vLines)
{
    char buf[5000];

    int nRead=read(nFd, buf, sizeof(buf));
    if (nRead > 0)
    {
        sBuffer.append(buf,nRead);

        //do we have at least \n
        size_t nLastLineBreak = sBuffer.find_last_of('\n');
        if(nLastLineBreak != std::string::npos)
        {
            std::string sComplete = sBuffer.substr(0, nLastLineBreak);
            vLines = SplitString(sComplete, '\n');

            if(nLastLineBreak != sBuffer.size()-1)
            {
                sBuffer = sBuffer.substr(nLastLineBreak+1);
            }
            else
            {
                sBuffer.clear();
            }
        }
        return true;
    }
    return false;
}



Launcher::Launcher(const std::filesystem::path& pathConfig, std::function<void(const std::string&, const Json::Value&)> statusCallback, std::function<void(const std::string&, int)> exitCallback) :
    m_pid(0),
    m_nPipe{-1,-1},
    m_pathConfig(pathConfig),
    m_nExitCode(0),
    m_sLoggerApp("/usr/local/bin/logger"),
    m_statusCallback(statusCallback),
    m_exitCallback(exitCallback)
{
    m_jsStatus["id"] = m_pathConfig.stem().string();
}

Launcher::~Launcher()
{
    CloseLogger();
}




bool Launcher::IsRunning() const
{
    return (m_pid!=0);
}

int Launcher::GetReadPipe() const
{
    return m_nPipe[READ];
}



int Launcher::LaunchLogger()
{

    pmlLog(pml::LOG_DEBUG) << "Launcher\tLaunchLogger: " << m_pathConfig;

    int nError = pipe(m_nPipe);
    if(nError != 0)
    {
       pmlLog(pml::LOG_ERROR) << "could not open pipe: " << strerror(nError) << std::endl;

        return PIPE_OPEN_ERROR;
    }

    m_pid = fork();
    if(m_pid < 0)
    {
        close(m_nPipe[WRITE]);
        close(m_nPipe[READ]);

        pmlLog(pml::LOG_ERROR) << "could not fork: " << strerror(nError) << std::endl;

        return FORK_ERROR;
    }
    else if(m_pid > 0)
    {   // Parent
        close(m_nPipe[WRITE]);  //close write end
        PipeRead();
        return m_nPipe[READ];
    }
    else
    {   //child

        close(m_nPipe[READ]);   //close read end
        dup2(m_nPipe[WRITE],STDOUT_FILENO); //redirect stdout to the pipe

        //redirect stderr to /dev/null
        int fderr = open("/dev/null", O_WRONLY);
        if(fderr >= 0)
        {
            dup2(fderr, STDERR_FILENO);
        }

        //create the args and launch the logger
        m_sConfigArg = m_pathConfig.string();
        char* args[] = {&m_sLoggerApp[0], &m_sConfigArg[0], nullptr};

        nError = execv(m_sLoggerApp.c_str(), args);

        if(nError)
        {
            std::cout << "Exec failed: " << m_sLoggerApp << std::endl;
            exit(-1);
        }
        return 0;
    }
}


bool Launcher::StopLogger()
{
    if(m_pid == 0)
    {
        pmlLog(pml::LOG_WARN) << "Stop logger "<< m_pathConfig << "- Logger not running" << std::endl;
        return false;
    }
    else
    {
        int nError = kill(m_pid, SIGTERM);
        if(nError != 0)
        {
            pmlLog(pml::LOG_ERROR) << "- Could not send signal to logger" << std::endl;
            return false;
        }
        else
        {
            pmlLog(pml::LOG_INFO) << "- Signal sent to logger" << std::endl;
            return true;
        }
    }

}

void Launcher::PipeRead()
{
    m_tpLastMessage = std::chrono::system_clock::now();
}

bool Launcher::ClosePipeIfInactive()
{
    //@todo possibly allow setting of timeout
    if((std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-m_tpLastMessage)).count() > 30)
    {
        CloseLogger();
        pmlLog(pml::LOG_ERROR) << m_pathConfig << "\tTimeout - closed" << std::endl;
        return true;
    }
    return false;
}

void Launcher::CloseLogger()
{
    if(m_pid != 0)
    {
        kill(m_pid, SIGKILL);
        int nStatus;
        waitpid(m_pid, &nStatus,0);
        close(m_nPipe[READ]);
        m_nPipe[READ] = PIPE_CLOSED;
    }
}

void Launcher::Read()
{
    PipeRead();

    std::vector<std::string> vOut;

    if(ReadFromPipe(m_nPipe[READ], m_sOut, vOut))
    {
        for(const auto& sLine : vOut)
        {
            auto jsValue = ConvertToJson(sLine);
            if(jsValue.isMember(jsonConsts::event) && jsValue.isMember(jsonConsts::data))
            {
                m_jsStatus[jsValue[jsonConsts::event].asString()] = jsValue[jsonConsts::data];
                CreateSummary();
            }
        }
        if(m_statusCallback)
        {
            m_statusCallback(m_pathConfig.stem(), m_jsStatus);
        }
    }
    else
    {
        CloseLogger();
        pmlLog(pml::LOG_ERROR) << m_pathConfig << "\tEOF or Read Error: Logger closed" << std::endl;
        LaunchLogger();
    }
}

void Launcher::CreateSummary()
{
    m_jsStatusSummary.clear();
    m_jsStatusSummary[jsonConsts::name] = m_pathConfig.stem().string();
    m_jsStatusSummary[jsonConsts::running] = IsRunning();

    if(m_jsStatus.isMember(jsonConsts::streaming))
    {
        m_jsStatusSummary[jsonConsts::streaming] = m_jsStatus[jsonConsts::streaming];
    }
    if(m_jsStatus.isMember(jsonConsts::session))
    {
        m_jsStatusSummary[jsonConsts::session] = m_jsStatus[jsonConsts::session][jsonConsts::name];
    }

    if(m_jsStatus.isMember(jsonConsts::heartbeat))
    {
        m_jsStatusSummary[jsonConsts::timestamp] = m_jsStatus[jsonConsts::heartbeat][jsonConsts::timestamp];
        m_jsStatusSummary[jsonConsts::up_time] = m_jsStatus[jsonConsts::heartbeat][jsonConsts::up_time];
    }

    if(m_jsStatus.isMember(jsonConsts::file) && m_jsStatus[jsonConsts::file].isMember(jsonConsts::filename))
    {
        std::filesystem::path file = m_jsStatus[jsonConsts::file][jsonConsts::filename].asString();
        m_jsStatusSummary[jsonConsts::filename] = file.stem().string();
    }

}
const Json::Value& Launcher::GetStatusSummary() const
{
    return m_jsStatusSummary;
}
