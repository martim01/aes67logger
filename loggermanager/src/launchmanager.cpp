#include "launchmanager.h"
#include "launcher.h"
#include "log.h"
#include "inimanager.h"
#include "jsonconsts.h"
#include "aes67utils.h"

using namespace std::placeholders;

LaunchManager::LaunchManager()=default;

LaunchManager::~LaunchManager()
{
    if(m_pThread)
    {
        m_context.stop();
        m_pThread->join();
    }
    //loggers will shutdown in the launcher destructor
}

void LaunchManager::Init(const iniManager& iniConfig, const std::function<void(const std::string&, const Json::Value&)>& statusCallback, 
const std::function<void(const std::string&, int, bool)>& exitCallback)
{
    m_pathLaunchers.assign(iniConfig.Get(jsonConsts::path, jsonConsts::loggers, "/usr/local/etc/loggers"));
    m_pathSockets.assign(iniConfig.Get(jsonConsts::path, jsonConsts::sockets, "/var/local/loggers/sockets"));
    m_pathAudio.assign(iniConfig.Get(jsonConsts::path, jsonConsts::audio, "/var/local/loggers/audio"));

    m_bUseTransmissionTime = iniConfig.GetBool(jsonConsts::logger, jsonConsts::useTransmission, false);
    m_sLoggerInterface = iniConfig.Get(jsonConsts::logger, jsonConsts::interface, "eth0");
    m_nHeartbeatGap = iniConfig.Get(jsonConsts::logger, jsonConsts::gap, 2500l);
    m_nAoipBuffer = iniConfig.Get(jsonConsts::logger, jsonConsts::buffer, 4096l);
    m_nLoggerConsoleLevel = iniConfig.Get(jsonConsts::logger, jsonConsts::console, -1l);
    m_nLoggerFileLevel = iniConfig.Get(jsonConsts::logger, jsonConsts::file, 2l);

    m_statusCallback = statusCallback;
    m_exitCallback = exitCallback;

    EnumLoggers();
    LaunchAll();

}

void LaunchManager::EnumLoggers()
{
    pmlLog() << "EnumLoggers...";

    std::scoped_lock<std::mutex> lg(m_mutex);

    try
    {
        m_mLaunchers.clear();

        for(const auto& entry : std::filesystem::directory_iterator(m_pathLaunchers))
        {
            if(entry.path().extension() == ".ini")
            {
                pmlLog() << "Logger: '" << entry.path().stem() << "' found";

                m_mLaunchers.try_emplace(entry.path().stem(), std::make_shared<Launcher>(m_context, entry.path(),
                                                                                     MakeSocketFullPath(entry.path().stem()),
                                                                                     m_statusCallback, std::bind(&LaunchManager::ExitCallback, this, _1,_2,_3)));
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

std::filesystem::path LaunchManager::MakeConfigFullPath(const std::string& sLogger) const
{
    auto path = m_pathLaunchers;
    path /= std::string(sLogger+".ini");
    return path;
}

std::filesystem::path LaunchManager::MakeSocketFullPath(const std::string& sLogger) const
{
    auto path = m_pathSockets;
    path /= sLogger;
    return path;
}

void LaunchManager::LaunchAll()
{
    std::scoped_lock<std::mutex> lg(m_mutex);

    pmlLog() << "Start all loggers";

    for(const auto& [sName, pLauncher] : m_mLaunchers)
    {
        LaunchLogger(pLauncher);
    }
    PipeThread();

}

void LaunchManager::LaunchLogger(std::shared_ptr<Launcher> pLauncher) const
{
    if(pLauncher->IsRunning() == false)
    {
        pLauncher->LaunchLogger();
    }
}


pml::restgoose::response LaunchManager::AddLogger(const pml::restgoose::response& theData)
{
    //check for the data

    if(CheckJsonMembers(theData.jsonData, {{jsonConsts::name, enumJsonType::STRING},
                                           {jsonConsts::source, enumJsonType::STRING},
                                           {jsonConsts::rtsp, enumJsonType::STRING},
                                           {jsonConsts::sdp, enumJsonType::STRING},
                                           {jsonConsts::wav, enumJsonType::NUMBER},
                                           {jsonConsts::opus, enumJsonType::NUMBER},
                                           {jsonConsts::flac, enumJsonType::NUMBER},
                                           {jsonConsts::filelength, enumJsonType::NUMBER}}) == false)
    {
        return pml::restgoose::response(400, "Json missing required parameters");
    }



    std::scoped_lock<std::mutex> lg(m_mutex);

    pmlLog() << "Add logger '" << theData.jsonData[jsonConsts::name].asString() << "'";

    if(m_mLaunchers.find(theData.jsonData[jsonConsts::name].asString()) != m_mLaunchers.end())
    {
        return pml::restgoose::response(409, "logger with name "+theData.jsonData[jsonConsts::name].asString()+" already exists");
    }

    CreateLoggerConfig(theData.jsonData);


    auto pLauncher = std::make_shared<Launcher>(m_context, MakeConfigFullPath(theData.jsonData[jsonConsts::name].asString()),
                                                           MakeSocketFullPath(theData.jsonData[jsonConsts::name].asString()),
                                                           m_statusCallback,
                                                           std::bind(&LaunchManager::ExitCallback, this, _1,_2,_3));


    m_mLaunchers.try_emplace(theData.jsonData[jsonConsts::name].asString(),pLauncher);
    LaunchLogger(pLauncher);


    pml::restgoose::response theResponse(200);
    theResponse.jsonData["logger"] = theData.jsonData[jsonConsts::name].asString();
    return theResponse;
}

void LaunchManager::CreateLoggerConfig(const Json::Value& jsData) const
{

    iniManager config;

    config.Set(jsonConsts::general, jsonConsts::name, jsData[jsonConsts::name].asString());

    if(CheckJsonMembers(jsData, {{jsonConsts::console, enumJsonType::NUMBER}}))
    {
        config.Set(jsonConsts::log, jsonConsts::console, jsData[jsonConsts::console].asInt());
    }
    else
    {
        config.Set(jsonConsts::log, jsonConsts::console, (int)m_nLoggerConsoleLevel);
    }
    if(CheckJsonMembers(jsData, {{jsonConsts::file, enumJsonType::NUMBER}}))
    {
        config.Set(jsonConsts::log, jsonConsts::file, jsData[jsonConsts::file].asInt());
    }
    else
    {
        config.Set(jsonConsts::log, jsonConsts::console, (int)m_nLoggerFileLevel);
    }
    config.Set(jsonConsts::path, jsonConsts::logs, m_sLogPath);
    config.Set(jsonConsts::path, jsonConsts::sockets, m_pathSockets);
    config.Set(jsonConsts::path, jsonConsts::audio, m_pathAudio);

    if(CheckJsonMembers(jsData, {{jsonConsts::gap, enumJsonType::NUMBER}}))
    {
        config.Set(jsonConsts::heartbeat, jsonConsts::gap, jsData[jsonConsts::gap].asInt());
    }
    else
    {
        config.Set(jsonConsts::heartbeat, jsonConsts::gap, (int)m_nHeartbeatGap);
    }

    if(CheckJsonMembers(jsData, {{jsonConsts::interface, enumJsonType::STRING}}))
    {
        config.Set(jsonConsts::aoip, jsonConsts::interface, jsData[jsonConsts::interface].asString());
    }
    else
    {
        config.Set(jsonConsts::aoip, jsonConsts::interface, m_sLoggerInterface);
    }
    if(CheckJsonMembers(jsData, {{jsonConsts::buffer, enumJsonType::NUMBER}}))
    {
        config.Set(jsonConsts::aoip, jsonConsts::aoip, jsData[jsonConsts::buffer].asInt());
    }
    else
    {
        config.Set(jsonConsts::aoip, jsonConsts::aoip,(int)m_nAoipBuffer);
    }
    config.Set(jsonConsts::aoip, jsonConsts::useTransmission, m_bUseTransmissionTime);


    if(jsData[jsonConsts::sdp].asString().empty())
    {
        config.Set(jsonConsts::source, jsonConsts::sdp, "");
    }
    else
    {
        auto pathSDP = m_pathSdp;
        pathSDP /= jsData[jsonConsts::sdp].asString();
        config.Set(jsonConsts::source, jsonConsts::sdp, pathSDP.string());
    }

    config.Set(jsonConsts::source, jsonConsts::name, jsData[jsonConsts::source].asString());
    config.Set(jsonConsts::source, jsonConsts::rtsp, jsData[jsonConsts::rtsp].asString());

    config.Set(jsonConsts::keep, jsonConsts::wav, jsData[jsonConsts::wav].asString());
    config.Set(jsonConsts::keep, jsonConsts::opus, jsData[jsonConsts::opus].asString());
    config.Set(jsonConsts::keep, jsonConsts::flac, jsData[jsonConsts::flac].asString());

    config.Set(jsonConsts::aoip, jsonConsts::filelength, jsData[jsonConsts::filelength].asString());

    config.Write(MakeConfigFullPath(jsData[jsonConsts::name].asString()));
}



pml::restgoose::response LaunchManager::RemoveLogger(const std::string& sName)
{
    pmlLog() << "Remove logger '" << sName << "'";

    if(auto itLogger = m_mLaunchers.find(sName); itLogger != m_mLaunchers.end())
    {
        itLogger->second->RemoveLogger();
        return pml::restgoose::response(200);
    }


    return pml::restgoose::response(404, "logger "+sName+" not found");
}


void LaunchManager::PipeThread()
{
    pmlLog() << "Pipe Thread check if running";
    if(m_pThread)
    {
        m_context.stop();
        m_pThread->join();

        m_pThread = nullptr;
    }

    pmlLog() << "Pipe Thread now start";
    m_pThread = std::make_unique<std::thread>([this]()
    {
        pmlLog() << "Pipe Thread started";

        auto work = asio::require(m_context.get_executor(), asio::execution::outstanding_work_t::tracked);


        if(m_context.stopped())
        {
            m_context.restart();
        }
        m_context.run();

        pmlLog() << "Pipe Thread stopped";
    });
}

pml::restgoose::response LaunchManager::GetLoggerConfig(const std::string& sName) const
{
    iniManager ini;
    if(ini.Read(MakeConfigFullPath(sName)))
    {
        pml::restgoose::response theResponse(200);
        theResponse.jsonData[jsonConsts::logs][jsonConsts::console] = ini.Get(jsonConsts::logs, jsonConsts::console, -1l);
        theResponse.jsonData[jsonConsts::logs][jsonConsts::file] = ini.Get(jsonConsts::logs, jsonConsts::file, 2l);
        theResponse.jsonData[jsonConsts::logs][jsonConsts::path] = ini.Get(jsonConsts::logs, jsonConsts::path, "/var/loggers/log");
        theResponse.jsonData[jsonConsts::heartbeat][jsonConsts::gap] = ini.Get(jsonConsts::heartbeat, jsonConsts::gap, 10000l);

        theResponse.jsonData[jsonConsts::aoip][jsonConsts::interface] = ini.Get(jsonConsts::aoip, jsonConsts::interface, "eth0");
        theResponse.jsonData[jsonConsts::aoip][jsonConsts::buffer] = ini.Get(jsonConsts::aoip, jsonConsts::buffer, 4096l);

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
    if(jsData.isArray())
    {
        iniManager ini;
        if(ini.Read(MakeConfigFullPath(sName)))
        {
            for(Json::ArrayIndex ai  = 0; ai < jsData.size(); ++ai)
            {
                if(jsData[ai].isMember("section") && jsData[ai].isMember("key") && jsData[ai].isMember("value"))
                {
                    if(jsData[ai]["section"].asString() == jsonConsts::source && jsData[ai]["key"].asString() == jsonConsts::sdp)
                    {
                        if(jsData[ai]["value"].asString().empty())
                        {
                            ini.Set(jsonConsts::source, jsonConsts::sdp, "");
                        }
                        else
                        {
                            auto pathSDP = m_pathSdp;
                            pathSDP /= jsData[ai]["value"].asString();
                            ini.Set(jsonConsts::source, jsonConsts::sdp, pathSDP.string());
                        }
                    }
                    else
                    {
                        ini.Set(jsData[ai]["section"].asString(), jsData[ai]["key"].asString(), jsData[ai]["value"].asString());
                    }
                }
            }
            ini.Write();
            return GetLoggerConfig(sName);
        }
        return pml::restgoose::response(500, "Could not read config file for logger");
    }
    return pml::restgoose::response(400, "Patch data is not a Json array");
}

Json::Value LaunchManager::GetStatusSummary() const
{
    Json::Value jsValue(Json::arrayValue);
    for(const auto& [sName, pLauncher] : m_mLaunchers)
    {
        jsValue.append(pLauncher->GetStatusSummary());
    }
    return jsValue;
}


pml::restgoose::response LaunchManager::RestartLogger(const std::string& sName)
{
    if(auto itLauncher = m_mLaunchers.find(sName); itLauncher != m_mLaunchers.end())
    {
        itLauncher->second->RestartLogger();
        return pml::restgoose::response(200);
    }
    return pml::restgoose::response(404, sName+" not found");
}

void LaunchManager::ExitCallback(const std::string& sLogger, int nExitCode, bool bRemove)
{
    pmlLog() << "Logger " << sLogger << " has exited with exit code " << nExitCode;
    if(bRemove)
    {
        m_mLaunchers.erase(sLogger);
    }
    m_exitCallback(sLogger, nExitCode, bRemove);
}
