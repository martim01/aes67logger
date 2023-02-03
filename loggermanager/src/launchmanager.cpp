#include "launchmanager.h"
#include "launcher.h"
#include "log.h"
#include "inimanager.h"
#include "jsonconsts.h"
#include "aoiputils.h"

using namespace std::placeholders;

LaunchManager::LaunchManager()
{

}

LaunchManager::~LaunchManager()
{
    if(m_pThread)
    {
        m_context.stop();
        m_bRun = false;
        m_pThread->join();
    }
    //loggers will shutdown in the launcher destructor
}

void LaunchManager::Init(const iniManager& iniConfig, std::function<void(const std::string&, const Json::Value&)> statusCallback, std::function<void(const std::string&, int)> exitCallback)
{
    m_pathLaunchers.assign(iniConfig.Get(jsonConsts::path, jsonConsts::loggers, "/usr/local/etc/loggers"));
    m_pathSockets.assign(iniConfig.Get(jsonConsts::path, jsonConsts::sockets, "/var/loggers/sockets"));
    m_pathAudio.assign(iniConfig.Get(jsonConsts::path, jsonConsts::audio, "/var/loggers/wav"));


    m_statusCallback = statusCallback;
    m_exitCallback = exitCallback;
    EnumLoggers();
    LaunchAll();

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

                m_mLaunchers.insert({entry.path().stem(), std::make_shared<Launcher>(m_context, entry.path(),
                                                                                     MakeSocketFullPath(entry.path().stem()),
                                                                                     m_statusCallback, std::bind(&LaunchManager::ExitCallback, this, _1,_2,_3))});
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

std::filesystem::path LaunchManager::MakeSocketFullPath(const std::string& sLogger)
{
    auto path = m_pathSockets;
    path /= sLogger;
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
    PipeThread();

}

void LaunchManager::LaunchLogger(std::shared_ptr<Launcher> pLauncher)
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


    auto pLauncher = std::make_shared<Launcher>(m_context, MakeConfigFullPath(theData.jsonData[jsonConsts::name].asString()),
                                                           MakeSocketFullPath(theData.jsonData[jsonConsts::name].asString()),
                                                           m_statusCallback,
                                                           std::bind(&LaunchManager::ExitCallback, this, _1,_2,_3));


    m_mLaunchers.insert({theData.jsonData[jsonConsts::name].asString(),pLauncher});
    LaunchLogger(pLauncher);


    pml::restgoose::response theResponse(200);
    theResponse.jsonData["logger"] = theData.jsonData[jsonConsts::name].asString();
    return theResponse;
}

void LaunchManager::CreateLoggerConfig(const Json::Value& jsData)
{
    auto fileSdp = m_pathSdp;
    fileSdp /= jsData[jsonConsts::name].asString();

    iniManager config;
    config.Set(jsonConsts::log, jsonConsts::console, jsData[jsonConsts::console].asInt());
    config.Set(jsonConsts::log, jsonConsts::file, jsData[jsonConsts::file].asInt());

    config.Set(jsonConsts::path, jsonConsts::logs, m_sLogPath+jsData[jsonConsts::name].asString());
    config.Set(jsonConsts::path, jsonConsts::sockets, m_pathSockets);
    config.Set(jsonConsts::path, jsonConsts::audio, m_pathAudio);

    config.Set(jsonConsts::heartbeat, jsonConsts::gap, jsData[jsonConsts::gap].asInt());
    config.Set(jsonConsts::aoip, jsonConsts::interface, jsData[jsonConsts::interface].asString());
    config.Set(jsonConsts::general, jsonConsts::name, jsData[jsonConsts::name].asString());
    config.Get(jsonConsts::aoip, jsonConsts::aoip, jsData[jsonConsts::buffer].asInt());
    if(jsData[jsonConsts::sdp].asString().empty())
    {
        config.Set(jsonConsts::source, jsonConsts::sdp,"");
        try
        {
            std::filesystem::remove(fileSdp);
        }
        catch(std::filesystem::filesystem_error& e)
        {
            pmlLog(pml::LOG_WARN) << "SDP file does not exist " << e.what();
        }
    }
    else
    {
        config.Set(jsonConsts::source, jsonConsts::sdp, fileSdp.string());
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

    config.Set(jsonConsts::source, jsonConsts::name, jsData[jsonConsts::source].asString());
    config.Get(jsonConsts::source, jsonConsts::rtsp, jsData[jsonConsts::rtsp].asString());

    config.Write(MakeConfigFullPath(jsData[jsonConsts::name].asString()));
}



pml::restgoose::response LaunchManager::RemoveLogger(const std::string& sName)
{
    pmlLog() << "Remove logger '" << sName << "'";

    auto itLogger = m_mLaunchers.find(sName);
    if(itLogger != m_mLaunchers.end())
    {
        itLogger->second->RemoveLogger();
        return pml::restgoose::response(200);
    }


    return pml::restgoose::response(404, "logger "+sName+" not found");
}



//void work(asio::io_context& context)
//{
//    asio::steady_timer timer(context);
//    timer.expires_from_now(std::chrono::seconds(1));
//    timer.async_wait([&context](const asio::error_code& e){ work(context); });
//}

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

        auto work = asio::require(m_context.get_executor(), asio::execution::outstanding_work.tracked);


        if(m_context.stopped())
        {
            m_context.restart();
        }
        m_context.run();

        pmlLog() << "Pipe Thread stopped";
    });
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
    //First check if we are updating the config or restarting the logger

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
    auto itLauncher = m_mLaunchers.find(sName);
    if(itLauncher != m_mLaunchers.end())
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
        //remove all endpoints for this logger
        //send out some status to say removed
    }
    m_exitCallback(sLogger, nExitCode);
}
