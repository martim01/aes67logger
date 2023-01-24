#include "launchmanager.h"
#include "launcher.h"
#include "log.h"
#include "inimanager.h"
#include "jsonconsts.h"
#include "aoiputils.h"


LaunchManager::LaunchManager()
{

}

LaunchManager::~LaunchManager()
{
    if(m_pThread)
    {
        m_bRun = false;
        m_pThread->join();
    }
    //loggers will shutdown in the launcher destructor
}

void LaunchManager::Init(const std::string& sPath, std::function<void(const std::string&, const Json::Value&)> statusCallback, std::function<void(const std::string&, int)> exitCallback)
{
    m_pathLaunchers.assign(sPath);

    m_statusCallback = statusCallback;
    m_exitCallback = exitCallback;
    EnumLoggers();
    PipeThread();
}

void LaunchManager::EnumLoggers()
{
    pmlLog() << "EnumLoggers...";

    std::lock_guard<std::mutex> lg(m_mutex);

    try
    {
        m_mLaunchers.clear();

        for(const auto& entry : std::filesystem::directory_iterator(m_pathLaunchers))
        {
            if(entry.path().extension() == ".ini")
            {
                pmlLog() << "Logger: '" << entry.path().stem() << "' found";

                m_mLaunchers.insert({entry.path().stem(), std::make_shared<Launcher>(entry.path(), m_statusCallback, m_exitCallback)});
            }
        }
        m_pathSdp = m_pathLaunchers;
        m_pathSdp /= "sdp";

        std::filesystem::create_directories(m_pathSdp);
    }
    catch(std::filesystem::filesystem_error& e)
    {
        pmlLog(pml::LOG_CRITICAL) << "Could not enum loggers..." << e.what();
    }
}

std::filesystem::path LaunchManager::MakeConfigFullPath(const std::string& sLogger)
{
    auto path = m_pathLaunchers;
    path /= std::string(sLogger+".ini");
    return path;
}

void LaunchManager::LaunchAll()
{
    std::lock_guard<std::mutex> lg(m_mutex);

    pmlLog() << "Start all loggers";

    for(const auto& [sName, pLauncher] : m_mLaunchers)
    {
        LaunchLogger(pLauncher);
    }
}

void LaunchManager::LaunchLogger(std::shared_ptr<Launcher> pLauncher)
{
    if(pLauncher->IsRecording() == false)
    {
        pLauncher->LaunchLogger();
    }
}

void LaunchManager::StopAll()
{
    std::lock_guard<std::mutex> lg(m_mutex);

    pmlLog() << "Stop all loggers";

    for(const auto& [sName, pLauncher] : m_mLaunchers)
    {
        pLauncher->StopLogger();
    }
}

pml::restgoose::response LaunchManager::AddLogger(const pml::restgoose::response& theData)
{
    //check for the data

    if(CheckJsonMembers(theData.jsonData, {{jsonConsts::name, enumJsonType::STRING},
                                           {jsonConsts::source, enumJsonType::STRING},
                                           {jsonConsts::rtsp, enumJsonType::STRING},
                                           {jsonConsts::sdp, enumJsonType::STRING},
                                           {jsonConsts::console, enumJsonType::NUMBER},
                                           {jsonConsts::file, enumJsonType::NUMBER},
                                           {jsonConsts::gap, enumJsonType::NUMBER},
                                           {jsonConsts::interface, enumJsonType::STRING},
                                           {jsonConsts::buffer, enumJsonType::NUMBER}}) == false)
    {
        return pml::restgoose::response(400, "Json missing required parameters");
    }



    std::lock_guard<std::mutex> lg(m_mutex);

    pmlLog() << "Add logger '" << theData.jsonData[jsonConsts::name].asString() << "'";
    auto itLogger = m_mLaunchers.find(theData.jsonData[jsonConsts::name].asString());
    if(itLogger != m_mLaunchers.end())
    {
        return pml::restgoose::response(409, "logger with name "+theData.jsonData[jsonConsts::name].asString()+" already exists");
    }

    CreateLoggerConfig(theData.jsonData);

    itLogger = m_mLaunchers.insert({theData.jsonData[jsonConsts::name].asString(), std::make_shared<Launcher>(MakeConfigFullPath(theData.jsonData[jsonConsts::name].asString()), m_statusCallback, m_exitCallback)}).first;
    LaunchLogger(itLogger->second);

    return pml::restgoose::response(200);
}

void LaunchManager::CreateLoggerConfig(const Json::Value& jsData)
{
    auto fileSdp = m_pathSdp;
    fileSdp /= jsData[jsonConsts::name].asString();

    iniManager config;
    config.Set("logs","console", jsData[jsonConsts::console].asInt());
    config.Set("logs","file", jsData[jsonConsts::file].asInt());

    config.Set("logs", "path", m_sLogPath+jsData[jsonConsts::name].asString());
    config.Set("heartbeat", "gap", jsData[jsonConsts::gap].asInt());
    config.Set("aoip", "interface", jsData[jsonConsts::interface].asString());
    config.Set("general", "name", jsData[jsonConsts::name].asString());
    config.Get("aoip", "buffer", jsData[jsonConsts::buffer].asInt());
    if(jsData[jsonConsts::sdp].asString().empty())
    {
        config.Set("source", "sdp","");
        std::filesystem::remove(fileSdp);
    }
    else
    {
        config.Set("source", "sdp", fileSdp.string());
        std::ofstream ofs(fileSdp.string(), std::fstream::trunc);
        if(ofs.is_open())
        {
            ofs << jsData[jsonConsts::sdp].asString();
        }
        else
        {
            pmlLog(pml::LOG_ERROR) << "Could not write to " << fileSdp;
        }

    }

    config.Set("source", "name", jsData[jsonConsts::source].asString());
    config.Get("source", "rtsp", jsData[jsonConsts::rtsp].asString());

    config.Write(MakeConfigFullPath(jsData[jsonConsts::name].asString()));
}



pml::restgoose::response LaunchManager::RemoveLogger(const std::string& sName)
{
    pmlLog() << "Remove logger '" << sName << "'";

    auto itLogger = m_mLaunchers.find(sName);
    if(itLogger != m_mLaunchers.end())
    {
        if(itLogger->second->IsRecording())
        {
            itLogger->second->StopLogger();
        }
        std::filesystem::remove(MakeConfigFullPath(sName));

        m_mLaunchers.erase(itLogger);
        return pml::restgoose::response(200);
    }
    return pml::restgoose::response(404, "logger "+sName+" not found");
}




void LaunchManager::PipeThread()
{
    if(m_pThread)
    {
        m_bRun = false;
        m_pThread->join();
        m_bRun = true;
        m_pThread = nullptr;
    }
    m_pThread = std::make_unique<std::thread>([this]()
    {
        fd_set read_set;
        timespec timeout = {1,0};

        while(m_bRun)
        {
            int nMaxFd = CreateReadSet(read_set);
            int nSelect = pselect(nMaxFd+1, &read_set, NULL, NULL, &timeout, NULL);
            if(nSelect == 0)
            {
                //no messages since timeout
                CheckForClosedLoggers();
            }
            else if(nSelect > 0)
            {
                ReadFromLoggers(read_set);
            }
            else    //error
            {
                pmlLog(pml::LOG_CRITICAL) << "PipeThreadtSelect Error" << std::endl;
                //break;
            }
        }
    });
}

int LaunchManager::CreateReadSet(fd_set& read_set)
{
    std::lock_guard<std::mutex> lg(m_mutex);

    int nMaxFd=0;
    FD_ZERO(&read_set);

    //create a read_set from all launchers with a read pipe
    for(const auto& [sName, pLauncher] : m_mLaunchers)
    {
        if(pLauncher->GetReadPipe() > 0)
        {
            nMaxFd = std::max(nMaxFd, pLauncher->GetReadPipe());
            FD_SET(pLauncher->GetReadPipe(), &read_set);
        }
    }
    return nMaxFd;
}

void LaunchManager::CheckForClosedLoggers()
{
    std::lock_guard<std::mutex> lg(m_mutex);

    //look at each logger and see when it last sent us a message
    for(auto& [sName, pLauncher] : m_mLaunchers)
    {
        if(pLauncher->ClosePipeIfInactive())
        {
            pLauncher->LaunchLogger();
        }
    }
}

void LaunchManager::ReadFromLoggers(fd_set& read_set)
{
    std::lock_guard<std::mutex> lg(m_mutex);
    for(auto& [sName, pLauncher] : m_mLaunchers)
    {
        if(FD_ISSET(pLauncher->GetReadPipe(), &read_set))
        {
            pLauncher->Read();
        }
    }
}

pml::restgoose::response LaunchManager::GetLoggerConfig(const std::string& sName)
{
    iniManager ini;
    if(ini.Read(MakeConfigFullPath(sName)))
    {
        pml::restgoose::response theResponse(200);
        theResponse.jsonData[jsonConsts::logs][jsonConsts::console] = ini.Get(jsonConsts::logs, jsonConsts::console, -1);
        theResponse.jsonData[jsonConsts::logs][jsonConsts::file] = ini.Get(jsonConsts::logs, jsonConsts::file, 2);
        theResponse.jsonData[jsonConsts::logs][jsonConsts::path] = ini.Get(jsonConsts::logs, jsonConsts::path, "/var/loggers/log");
        theResponse.jsonData[jsonConsts::heartbeat][jsonConsts::gap] = ini.Get(jsonConsts::heartbeat, jsonConsts::gap, 10000);

        theResponse.jsonData[jsonConsts::aoip][jsonConsts::interface] = ini.Get(jsonConsts::aoip, jsonConsts::interface, "eth0");
        theResponse.jsonData[jsonConsts::aoip][jsonConsts::buffer] = ini.Get(jsonConsts::aoip, jsonConsts::buffer, 4096);

        theResponse.jsonData[jsonConsts::general][jsonConsts::name] = ini.Get(jsonConsts::general, jsonConsts::name, sName);

        theResponse.jsonData[jsonConsts::source][jsonConsts::sdp] = ini.Get(jsonConsts::source, jsonConsts::sdp, "");
        theResponse.jsonData[jsonConsts::source][jsonConsts::name] = ini.Get(jsonConsts::source, jsonConsts::name, "");
        theResponse.jsonData[jsonConsts::source][jsonConsts::rtsp] = ini.Get(jsonConsts::source, jsonConsts::rtsp, "");


        return theResponse;
    }
    else
    {
        return pml::restgoose::response(404, "Config not found");
    }
}

pml::restgoose::response LaunchManager::UpdateLoggerConfig(const std::string& sName, const Json::Value& jsData)
{
    iniManager ini;
    if(ini.Read(MakeConfigFullPath(sName)))
    {
        ini.Set(jsonConsts::logs, jsonConsts::console, ExtractValueFromJson(jsData, {jsonConsts::logs, jsonConsts::console}, ini.Get(jsonConsts::logs, jsonConsts::console, -1)));

        ini.Set(jsonConsts::logs, jsonConsts::file, ExtractValueFromJson(jsData, {jsonConsts::logs, jsonConsts::file}, ini.Get(jsonConsts::logs, jsonConsts::file, 2)));
        ini.Set(jsonConsts::logs, jsonConsts::path, ExtractValueFromJson(jsData, {jsonConsts::logs, jsonConsts::path}, ini.Get(jsonConsts::logs, jsonConsts::path, "/var/loggers/log")));

        ini.Set(jsonConsts::heartbeat, jsonConsts::gap, ExtractValueFromJson(jsData, {jsonConsts::heartbeat, jsonConsts::gap}, ini.Get(jsonConsts::heartbeat, jsonConsts::gap, 10000)));

        ini.Set(jsonConsts::aoip, jsonConsts::interface, ExtractValueFromJson(jsData, {jsonConsts::aoip, jsonConsts::interface}, ini.Get(jsonConsts::aoip, jsonConsts::interface, "eth0")));
        ini.Set(jsonConsts::aoip, jsonConsts::buffer, ExtractValueFromJson(jsData, {jsonConsts::aoip, jsonConsts::buffer}, ini.Get(jsonConsts::aoip, jsonConsts::buffer, 4096)));

        ini.Set(jsonConsts::general, jsonConsts::name, ExtractValueFromJson(jsData, {jsonConsts::general, jsonConsts::name}, ini.Get(jsonConsts::general, jsonConsts::name, sName)));

        ini.Set(jsonConsts::source, jsonConsts::name, ExtractValueFromJson(jsData, {jsonConsts::source, jsonConsts::name}, ini.Get(jsonConsts::source, jsonConsts::name, "")));
        ini.Set(jsonConsts::source, jsonConsts::sdp, ExtractValueFromJson(jsData, {jsonConsts::source, jsonConsts::sdp}, ini.Get(jsonConsts::source, jsonConsts::sdp, "")));
        ini.Set(jsonConsts::source, jsonConsts::rtsp, ExtractValueFromJson(jsData, {jsonConsts::source, jsonConsts::rtsp}, ini.Get(jsonConsts::source, jsonConsts::rtsp, "")));

        ini.Write();
    }
    return GetLoggerConfig(sName);
}


