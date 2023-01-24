#include "server.h"
#include "sysinfomanager.h"
#include "json/json.h"
#include <sstream>
#include "log.h"
#include "logtofile.h"
#include <spawn.h>
#include <sys/wait.h>
#include <sys/types.h>
//#include "epi_version.h"
#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include "proccheck.h"
#include <chrono>
#include "aoiputils.h"
#include "launcher.h"
#include "jsonconsts.h"
#include "loggermanager_version.h"

using namespace std::placeholders;
using namespace pml;

const std::string Server::ROOT        = "/";             //GET
const std::string Server::API         = "x-api";          //GET
const std::string Server::LOGGERS     = "loggers";
const std::string Server::POWER       = "power";
const std::string Server::CONFIG      = "config";
const std::string Server::STATUS      = "status";
const std::string Server::INFO        = "info";
const std::string Server::UPDATE      = "update";
const std::string Server::WS          = "ws";        //GET PUT

const endpoint Server::EP_ROOT        = endpoint("");
const endpoint Server::EP_API         = endpoint(ROOT+API);
const endpoint Server::EP_LOGGERS     = endpoint(EP_API.Get()+"/"+LOGGERS);
const endpoint Server::EP_POWER       = endpoint(EP_API.Get()+"/"+POWER);
const endpoint Server::EP_CONFIG      = endpoint(EP_API.Get()+"/"+CONFIG);
const endpoint Server::EP_UPDATE      = endpoint(EP_API.Get()+"/"+UPDATE);
const endpoint Server::EP_INFO        = endpoint(EP_API.Get()+"/"+INFO);
const endpoint Server::EP_WS          = endpoint(EP_API.Get()+"/"+WS);
const endpoint Server::EP_WS_LOGGERS  = endpoint(EP_WS.Get()+"/"+LOGGERS);
const endpoint Server::EP_WS_INFO     = endpoint(EP_WS.Get()+"/"+INFO);
const endpoint Server::EP_WS_STATUS   = endpoint(EP_WS.Get()+"/"+STATUS);


static pml::restgoose::response ConvertPostDataToJson(const postData& vData)
{
    pml::restgoose::response resp(404, "No data sent or incorrect data sent");
    if(vData.size() == 1)
    {
        resp.nHttpCode = 200;
        resp.jsonData = ConvertToJson(vData[0].data.Get());
    }
    else if(vData.size() > 1)
    {
        resp.nHttpCode = 200;
        resp.jsonData.clear();
        for(size_t i = 0; i < vData.size(); i++)
        {
            if(vData[i].name.Get().empty() == false)
            {
                if(vData[i].filepath.Get().empty() == true)
                {
                    resp.jsonData[vData[i].name.Get()] = vData[i].data.Get();
                }
                else
                {
                    resp.jsonData[vData[i].name.Get()][jsonConsts::name] = vData[i].data.Get();
                    resp.jsonData[vData[i].name.Get()][jsonConsts::location] = vData[i].filepath.Get();
                }
            }
        }
    }
    return resp;
}


Server::Server() :
   m_nTimeSinceLastCall(0),
   m_nLogToConsole(-1),
   m_nLogToFile(-1),
   m_bLoggedThisHour(false)
{
    GetInitialLoggerStatus();
}


void Server::GetInitialLoggerStatus()
{
//    if(IsProcRunning("logger"))
//    {   //just started up so must be an orphaned player
//        m_jsStatus["player"] = "Orphaned";
//    }
//    else
//    {
//        m_jsStatus["player"] ="Stopped";
//    }
}

void Server::InitLogging()
{
    if(m_config.Get("logging", "console", 0) > -1 )
    {
        if(m_nLogToConsole == -1)
        {
            m_nLogToConsole = pmlLog().AddOutput(std::unique_ptr<pml::LogOutput>(new pml::LogOutput()));
        }
        pmlLog().SetOutputLevel(m_nLogToConsole, pml::enumLevel(m_config.Get("logging", "console", pml::LOG_TRACE)));
    }
    else if(m_nLogToConsole != -1)
    {
        pmlLog().RemoveOutput(m_nLogToConsole);
        m_nLogToConsole = -1;
    }

    if(m_config.Get("logging", "file", pml::LOG_INFO) > -1)
    {
        if(m_nLogToFile == -1)
        {
            m_nLogToFile = pmlLog().AddOutput(std::make_unique<pml::LogToFile>(CreatePath(m_config.Get("paths","logs","."))+"loggermanager"));
        }
        pmlLog().SetOutputLevel(m_nLogToFile, pml::enumLevel(m_config.Get("logging", "file", pml::LOG_INFO)));
    }
    else if(m_nLogToFile != -1)
    {
        pmlLog().RemoveOutput(m_nLogToFile);
        m_nLogToFile = -1;
    }

}

void Server::Run(const std::string& sConfigFile)
{
    if(m_config.Read(sConfigFile) == false)
    {
        pmlLog().AddOutput(std::unique_ptr<pml::LogOutput>(new pml::LogOutput()));
        pmlLog(pml::LOG_CRITICAL) << "Could not open '" << sConfigFile << "' exiting.";
        return;
    }

    InitLogging();

    pmlLog() << "Core\tStart" ;


    m_info.SetDiskPath(m_config.Get("paths", "audio", "/var/loggers"));

    if(m_server.Init(fileLocation(m_config.Get("api", "sslCert", "")), fileLocation(m_config.Get("api", "ssKey", "")), ipAddress("0.0.0.0"), m_config.Get("api", "port", 8080), EP_API, true))
    {
        m_server.SetStaticDirectory("/home/matt/aes67logger/www");
        //add luauncher callbacks
        m_launcher.Init(m_config.Get("paths", "loggers", "/usr/local/etc/loggers"), std::bind(&Server::StatusCallback, this, _1,_2), std::bind(&Server::ExitCallback, this, _1,_2));

        //add server callbacks
        CreateEndpoints();

        //start the server loop
        m_server.Run(false, std::chrono::milliseconds(50));
    }
}

bool Server::CreateEndpoints()
{

    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "CreateEndpoints" ;

    //m_server.AddEndpoint(pml::restgoose::GET, EP_ROOT, std::bind(&Server::GetRoot, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::GET, EP_API, std::bind(&Server::GetApi, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_LOGGERS, std::bind(&Server::GetLoggers, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::POST, EP_LOGGERS, std::bind(&Server::PostLogger, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_CONFIG, std::bind(&Server::GetConfig, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::PATCH, EP_CONFIG, std::bind(&Server::PatchConfig, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_POWER, std::bind(&Server::GetPower, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::PUT, EP_POWER, std::bind(&Server::PutPower, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_INFO, std::bind(&Server::GetInfo, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_UPDATE, std::bind(&Server::GetUpdate, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::PUT, EP_UPDATE, std::bind(&Server::PutUpdate, this, _1,_2,_3,_4));


    m_server.AddWebsocketEndpoint(EP_WS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
    m_server.AddWebsocketEndpoint(EP_WS_INFO, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
    m_server.AddWebsocketEndpoint(EP_WS_STATUS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));

    //now add all the dynamic methodpoints
    for(const auto& [sName, pLauncher] : m_launcher.GetLaunchers())
    {
        m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&Server::GetLogger, this, _1,_2,_3,_4));
        m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+STATUS), std::bind(&Server::GetLoggerStatus, this, _1,_2,_3,_4));
        m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG), std::bind(&Server::GetLoggerConfig, this, _1,_2,_3,_4));

        m_server.AddEndpoint(pml::restgoose::HTTP_DELETE, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&Server::DeleteLogger, this, _1,_2,_3,_4));

        m_server.AddEndpoint(pml::restgoose::PUT, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG), std::bind(&Server::PutLoggerConfig, this, _1,_2,_3,_4));

        m_server.AddWebsocketEndpoint(endpoint(EP_WS_LOGGERS.Get()+"/"+sName), std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
    }




    //Add the loop callback function
    m_server.SetLoopCallback(std::bind(&Server::LoopCallback, this, _1));

    return true;
}

pml::restgoose::response Server::GetRoot(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetRoot" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(API);
    return theResponse;
}

pml::restgoose::response Server::GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetApi" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(LOGGERS);
    theResponse.jsonData.append(POWER);
    theResponse.jsonData.append(CONFIG);
    theResponse.jsonData.append(INFO);
    theResponse.jsonData.append(UPDATE);
    return theResponse;
}

pml::restgoose::response Server::GetLoggers(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetLoggers" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    for(const auto& [sName, pLauncher] : m_launcher.GetLaunchers())
    {
        theResponse.jsonData.append(sName);
    }
    return theResponse;
}

pml::restgoose::response Server::GetLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pml::restgoose::response theResponse(400);
    auto vPath = SplitString(theEndpoint.Get(),'/');
    auto itLogger = m_launcher.GetLaunchers().find(vPath.back());
    if(itLogger != m_launcher.GetLaunchers().end())
    {
        theResponse.nHttpCode = 200;
        theResponse.jsonData.append(CONFIG);
        theResponse.jsonData.append(STATUS);
    }
    else
    {
        theResponse.jsonData[jsonConsts::reason].append("Logger '"+vPath.back() + "' not found");
    }
    return theResponse;
}

pml::restgoose::response Server::GetLoggerStatus(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog() << "Server::GetLogger " << theEndpoint;
    pml::restgoose::response theResponse(400);
    auto vPath = SplitString(theEndpoint.Get(),'/');
    auto itLogger = m_launcher.GetLaunchers().find(vPath[vPath.size()-2]);
    if(itLogger != m_launcher.GetLaunchers().end())
    {
        theResponse.nHttpCode = 200;
        theResponse.jsonData = itLogger->second->GetJsonStatus();
    }
    else
    {
        theResponse.jsonData[jsonConsts::reason].append("Logger '"+vPath.back() + "' not found");
    }
    return theResponse;
}

pml::restgoose::response Server::GetLoggerConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    auto vPath = SplitString(theEndpoint.Get(),'/');
    return m_launcher.GetLoggerConfig(vPath[vPath.size()-2]);
}


pml::restgoose::response Server::PostLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode == 200)
    {
        theResponse = m_launcher.AddLogger(theResponse);
    }
    return theResponse;
}

pml::restgoose::response Server::DeleteLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{

    auto vUrl = SplitString(theEndpoint.Get(), '/');
    return m_launcher.RemoveLogger(vUrl.back());
}

pml::restgoose::response Server::PutLoggerConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode == 200)
    {
        auto vPath = SplitString(theEndpoint.Get(),'/');
        theResponse = m_launcher.UpdateLoggerConfig(vPath[vPath.size()-2], theResponse.jsonData);
    }

    return theResponse;
}

pml::restgoose::response Server::PutUpdate(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pml::restgoose::response theResponse(405, "not written yet");
    return theResponse;
}





pml::restgoose::response Server::GetConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetConfig" ;
    pml::restgoose::response theResponse;

    char host[256];
    gethostname(host, 256);
    theResponse.jsonData[jsonConsts::server][jsonConsts::hostname] = host;
    theResponse.jsonData[jsonConsts::server][jsonConsts::ip_address][jsonConsts::eth0] = GetIpAddress(jsonConsts::eth0);

    for(const auto& [sName, pSection] : m_config.GetSections())
    {
        if(sName != "restricted")
        {
            theResponse.jsonData[sName] = Json::Value(Json::objectValue);
            for(const auto& [sKey, sValue] : pSection->GetData())
            {
                theResponse.jsonData[sName][sKey] = sValue;
            }
        }
    }

    return theResponse;
}

pml::restgoose::response Server::GetUpdate(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    //get all the version numbers...
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetUpdate" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::rev] = pml::loggermanager::GIT_REV;
    theResponse.jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::tag] = pml::loggermanager::GIT_TAG;
    theResponse.jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::branch] = pml::loggermanager::GIT_BRANCH;
    theResponse.jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::date] = pml::loggermanager::GIT_DATE;
    theResponse.jsonData[jsonConsts::server][jsonConsts::major] = pml::loggermanager::VERSION_MAJOR;
    theResponse.jsonData[jsonConsts::server][jsonConsts::minor] = pml::loggermanager::VERSION_MINOR;
    theResponse.jsonData[jsonConsts::server][jsonConsts::patch] = pml::loggermanager::VERSION_PATCH;
    theResponse.jsonData[jsonConsts::server][jsonConsts::version] = pml::loggermanager::VERSION_STRING;

    theResponse.jsonData[jsonConsts::logger] = ConvertToJson(Exec("logger -v"));

    //get versions of other applications...
    return theResponse;
}


pml::restgoose::response Server::GetInfo(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetInfo" ;

    pml::restgoose::response theResponse;
    theResponse.jsonData = m_info.GetInfo();


    return theResponse;
}

pml::restgoose::response Server::GetPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetPower" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData[jsonConsts::status] = "On";

    return theResponse;
}


pml::restgoose::response Server::PutPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "PutPower: ";

    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode != 200)
    {
        return theResponse;
    }

    if(theResponse.jsonData.isMember(jsonConsts::command) == false || theResponse.jsonData[jsonConsts::command].isString() == false)
    {
        theResponse.nHttpCode = 400;
        theResponse.jsonData[jsonConsts::reason].append("No command sent");
        pmlLog(pml::LOG_ERROR) << " no command sent" ;
    }
    else if(CmpNoCase(theResponse.jsonData[jsonConsts::command].asString(), "restart server"))
    {
        // send a message to "main" to exit the loop
        pmlLog(pml::LOG_INFO) << " restart server" ;
        m_server.Stop();
    }
    else if(CmpNoCase(theResponse.jsonData[jsonConsts::command].asString(), "restart os"))
    {
        pmlLog(pml::LOG_INFO) << " restart os" ;

        theResponse = Reboot(LINUX_REBOOT_CMD_RESTART);

    }
    else if(CmpNoCase(theResponse.jsonData[jsonConsts::command].asString(), "shutdown"))
    {
        pmlLog(pml::LOG_INFO) << " shutdown" ;

        theResponse = Reboot(LINUX_REBOOT_CMD_POWER_OFF);
    }
    else
    {
        theResponse.nHttpCode = 400;
        theResponse.jsonData[jsonConsts::success] = false;
        theResponse.jsonData[jsonConsts::reason].append("Invalid command sent");
        pmlLog(pml::LOG_ERROR) << "'" << theResponse.jsonData[jsonConsts::command].asString() <<"' is not a valid command" ;
    }

    return theResponse;
}

pml::restgoose::response Server::PatchConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "PatchConfig" ;

    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode != 200)
    {
        return theResponse;
    }

    if(theResponse.jsonData.isObject() == false)
    {
        theResponse.nHttpCode  = 400;
        theResponse.jsonData[jsonConsts::success] = false;
        theResponse.jsonData[jsonConsts::reason].append("Invalid JSON");
    }
    else
    {
        m_config.ReRead();    //sync with on disk version

        for(auto const& sSection : theResponse.jsonData.getMemberNames())
        {
            if(theResponse.jsonData[sSection].isObject())
            {
                if(sSection == "server")
                {
                    PatchServerConfig(theResponse.jsonData[sSection]);
                }
                else if(sSection != "restricted" && sSection != "remove")
                {
                    auto pSection = m_config.GetSection(sSection);
                    if(pSection)
                    {
                        for(auto const& sKey : theResponse.jsonData[sSection].getMemberNames())
                        {
                            if(pSection->FindData(sKey) != pSection->GetDataEnd() && theResponse.jsonData[sSection][sKey].isConvertibleTo(Json::stringValue))
                            {
                                pSection->Set(sKey, theResponse.jsonData[sSection][sKey].asString());
                            }
                        }
                    }
                }
            }
        }
        m_config.Write();
        InitLogging();  //change logging if necessary
        //m_manager.InitPaths();  //change paths if necessary
        theResponse = GetConfig(theQuery, vData, theEndpoint, theUser);
        theResponse.jsonData[jsonConsts::success] = true;
    }
    return theResponse;
}


void Server::PatchServerConfig(const Json::Value& jsData)
{
    for(auto const& sKey : jsData.getMemberNames())
    {
        if(sKey == "hostname" && jsData[sKey].isString())
        {
            if(sethostname(jsData[sKey].asString().c_str(), jsData[sKey].asString().length()) != 0)
            {
                pmlLog(pml::LOG_WARN) << "Failed to set hostname";
            }
        }
    }
}

void Server::StatusCallback(const std::string& sLoggerId, const Json::Value& jsStatus)
{
    //lock as jsStatus can be called by pipe thread and server thread
    std::lock_guard<std::mutex> lg(m_mutex);
    m_server.SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLoggerId)}, jsStatus);
}

void Server::ExitCallback(const std::string& sLoggerId, int nExit)
{
    pmlLog() << "Logger exited" ;

    //lock as jsStatus can be called by pipe thread and server thread
    std::lock_guard<std::mutex> lg(m_mutex);

    auto jsStatus = Json::Value(Json::objectValue);    //reset

    jsStatus[jsonConsts::id] = sLoggerId;
    jsStatus[jsonConsts::status] = jsonConsts::stopped;

    if(WIFEXITED(nExit))
    {
        jsStatus[jsonConsts::exit][jsonConsts::code] = WEXITSTATUS(nExit);
    }
    if(WIFSIGNALED(nExit))
    {
        jsStatus[jsonConsts::exit][jsonConsts::signal][jsonConsts::code] = WTERMSIG(nExit);
        jsStatus[jsonConsts::exit][jsonConsts::signal][jsonConsts::description] = strsignal(WTERMSIG(nExit));
        if(WCOREDUMP(nExit))
        {
            jsStatus[jsonConsts::exit][jsonConsts::core_dump] = true;
        }
    }
    if(WIFSTOPPED(nExit))
    {
        jsStatus[jsonConsts::stopped][jsonConsts::signal] = WSTOPSIG(nExit);
    }
    if(WIFCONTINUED(nExit))
    {
        jsStatus[jsonConsts::resumed][jsonConsts::signal] = WSTOPSIG(nExit);
    }

    m_server.SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLoggerId)}, m_jsStatus);
}


void Server::LoopCallback(std::chrono::milliseconds durationSince)
{
    m_nTimeSinceLastCall += durationSince.count();


    if(m_nTimeSinceLastCall > 2000)
    {
        m_server.SendWebsocketMessage({EP_WS_INFO}, m_info.GetInfo());
        m_server.SendWebsocketMessage({EP_WS_STATUS}, m_launcher.GetStatusSummary());
        m_nTimeSinceLastCall = 0;
    }

    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    tm local_time = *localtime(&now);
    if(local_time.tm_min == 0 && local_time.tm_sec == 0)
    {
        if(m_bLoggedThisHour == false)
        {
            m_bLoggedThisHour = true;
            pmlLog() << GetCurrentTimeAsIsoString() ;
        }
    }
    else
    {
        m_bLoggedThisHour = false;
    }
}



pml::restgoose::response Server::Reboot(int nCommand)
{
    pml::restgoose::response theResponse;
    sync(); //make sure filesystem is synced
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int nError = reboot(nCommand);
    if(nError == -1)
    {
        theResponse.nHttpCode = 500;
        theResponse.jsonData[jsonConsts::success] = false;
        theResponse.jsonData[jsonConsts::reason].append(strerror(errno));
    }
    else
    {
        theResponse.jsonData[jsonConsts::success] = true;
    }
    return theResponse;
}

bool Server::WebsocketAuthenticate(const endpoint& theEndpoint, const query& theQuery, const userName& theUser, const ipAddress& peer)
{
    pmlLog() << "Websocket connection request from " << peer;
    return true;
}

bool Server::WebsocketMessage(const endpoint& theEndpoint, const Json::Value& jsData)
{
    pmlLog() << "Websocket message '" << jsData << "'";
    return true;
}

void Server::WebsocketClosed(const endpoint& theEndpoint, const ipAddress& peer)
{
    pmlLog() << "Websocket closed from " << peer;
}

