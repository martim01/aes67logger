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
#include <iomanip>

#include "loggermanager_version.h"

using namespace std::placeholders;
using namespace pml;

const std::string Server::ROOT        = "/";             //GET
const std::string Server::API         = "x-api";          //GET
const std::string Server::LOGIN       = "login";
const std::string Server::LOGGERS     = "loggers";
const std::string Server::POWER       = "power";
const std::string Server::CONFIG      = "config";
const std::string Server::STATUS      = "status";
const std::string Server::INFO        = "info";
const std::string Server::UPDATE      = "update";
const std::string Server::LOGS        = "logs";
const std::string Server::WS          = "ws";        //GET PUT

const endpoint Server::EP_ROOT        = endpoint("");
const endpoint Server::EP_API         = endpoint(ROOT+API);
const endpoint Server::EP_LOGIN       = endpoint(EP_API.Get()+"/"+LOGIN);
const endpoint Server::EP_LOGGERS     = endpoint(EP_API.Get()+"/"+LOGGERS);
const endpoint Server::EP_POWER       = endpoint(EP_API.Get()+"/"+POWER);
const endpoint Server::EP_CONFIG      = endpoint(EP_API.Get()+"/"+CONFIG);
const endpoint Server::EP_UPDATE      = endpoint(EP_API.Get()+"/"+UPDATE);
const endpoint Server::EP_INFO        = endpoint(EP_API.Get()+"/"+INFO);
const endpoint Server::EP_LOGS        = endpoint(EP_API.Get()+"/"+LOGS);
const endpoint Server::EP_WS          = endpoint(EP_API.Get()+"/"+WS);
const endpoint Server::EP_WS_LOGGERS  = endpoint(EP_WS.Get()+"/"+LOGGERS);
const endpoint Server::EP_WS_INFO     = endpoint(EP_WS.Get()+"/"+INFO);
const endpoint Server::EP_WS_STATUS   = endpoint(EP_WS.Get()+"/"+STATUS);


static pml::restgoose::response ConvertPostDataToJson(const postData& vData)
{
    pml::restgoose::response resp(404, "No data sent or incorrect data sent");
    if(vData.size() == 1)
    {
        auto js = ConvertToJson(vData[0].data.Get());
        if(js)
        {
            resp.nHttpCode = 200;
            resp.jsonData = *js;
        }
        else
        {
            resp.nHttpCode = 400;
        }
    }
    else if(vData.size() > 1)
    {
        resp.nHttpCode = 200;
        resp.jsonData.clear();
        for(size_t i = 0; i < vData.size(); i++)
        {
            pmlLog() << "ConvertPostDataToJson: data " << i <<"=" << vData[i].data.Get();
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
            std::filesystem::path pathLog = m_config.Get(jsonConsts::path,jsonConsts::log,".");
            pathLog /= "loggermanager";
            m_nLogToFile = pmlLog().AddOutput(std::make_unique<pml::LogToFile>(pathLog));
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


    m_info.SetDiskPath(m_config.Get(jsonConsts::path, jsonConsts::audio, "/var/loggers"));

    if(m_server.Init(fileLocation(m_config.Get(jsonConsts::api, "sslCert", "")), fileLocation(m_config.Get(jsonConsts::api, "ssKey", "")), ipAddress("0.0.0.0"), m_config.Get(jsonConsts::api, "port", 8080), EP_API, true,true))
    {

        m_server.SetAuthorizationTypeBearer(std::bind(&Server::AuthenticateToken, this, _1), std::bind(&Server::RedirectToLogin, this), true);
        m_server.SetUnprotectedEndpoints({methodpoint(pml::restgoose::GET, endpoint("")),
                                          methodpoint(pml::restgoose::GET, endpoint("/index.html")),
                                          methodpoint(pml::restgoose::POST, EP_LOGIN),
                                          methodpoint(pml::restgoose::GET, endpoint("/js/*")),
                                          methodpoint(pml::restgoose::GET, endpoint("/uikit/*")),
                                          methodpoint(pml::restgoose::GET, endpoint("/images/*"))});

        m_server.SetStaticDirectory(m_config.Get(jsonConsts::api,jsonConsts::static_pages, "/var/www"));


        //add luauncher callbacks
        m_launcher.Init(m_config, std::bind(&Server::StatusCallback, this, _1,_2), std::bind(&Server::ExitCallback, this, _1,_2));

        //add server callbacks
        CreateEndpoints();

        //start the server loop
        m_server.Run(false, std::chrono::milliseconds(50));

        pmlLog() << "Core\tStop" ;
        DeleteEndpoints();
    }
}

bool Server::CreateEndpoints()
{

    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "CreateEndpoints" ;

    m_server.AddEndpoint(pml::restgoose::POST, EP_LOGIN, std::bind(&Server::PostLogin, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::HTTP_DELETE, EP_LOGIN, std::bind(&Server::DeleteLogin, this, _1,_2,_3,_4));
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

    m_server.AddEndpoint(pml::restgoose::GET, EP_LOGS, std::bind(&Server::GetLogs, this, _1,_2,_3,_4));

    m_server.AddWebsocketEndpoint(EP_WS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
    m_server.AddWebsocketEndpoint(EP_WS_INFO, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
    m_server.AddWebsocketEndpoint(EP_WS_STATUS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
    m_server.AddWebsocketEndpoint(EP_WS_LOGGERS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));

    //now add all the dynamic methodpoints
    for(const auto& [sName, pLauncher] : m_launcher.GetLaunchers())
    {
        AddLoggerEndpoints(sName);
    }




    //Add the loop callback function
    m_server.SetLoopCallback(std::bind(&Server::LoopCallback, this, _1));

    return true;
}

void Server::AddLoggerEndpoints(const std::string& sName)
{
    m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&Server::GetLogger, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+STATUS), std::bind(&Server::GetLoggerStatus, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG), std::bind(&Server::GetLoggerConfig, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::HTTP_DELETE, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&Server::DeleteLogger, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::PUT, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG), std::bind(&Server::PutLoggerConfig, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::PUT, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&Server::PutLoggerPower, this, _1,_2,_3,_4));

    m_server.AddWebsocketEndpoint(endpoint(EP_WS_LOGGERS.Get()+"/"+sName), std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
}

void Server::DeleteEndpoints()
{

    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "DeleteEndpoints" ;

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_API);

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_LOGGERS);
    m_server.DeleteEndpoint(pml::restgoose::POST, EP_LOGGERS);

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_CONFIG);
    m_server.DeleteEndpoint(pml::restgoose::PATCH, EP_CONFIG);

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_POWER);
    m_server.DeleteEndpoint(pml::restgoose::PUT, EP_POWER);

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_INFO);

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_UPDATE);
    m_server.DeleteEndpoint(pml::restgoose::PUT, EP_UPDATE);

    //now Delete all the dynamic methodpoints
    for(const auto& [sName, pLauncher] : m_launcher.GetLaunchers())
    {
        m_server.DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName));
        m_server.DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+STATUS));
        m_server.DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG));

        m_server.DeleteEndpoint(pml::restgoose::HTTP_DELETE, endpoint(EP_LOGGERS.Get()+"/"+sName));
        m_server.DeleteEndpoint(pml::restgoose::PUT, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG));

    }
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

pml::restgoose::response Server::PutLoggerPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    auto vPath = SplitString(theEndpoint.Get(),'/');
    return m_launcher.RestartLogger(vPath[vPath.size()-1]);

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
    theResponse.jsonData[jsonConsts::server][jsonConsts::date] = pml::loggermanager::BUILD_DATE;

    auto js = ConvertToJson(Exec("logger -v"));
    if(js)
    {
        theResponse.jsonData[jsonConsts::logger] = *js;
    }

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




pml::restgoose::response Server::GetLogs(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pml::restgoose::response theResponse(404, "Log not defined");

    auto itLogger = theQuery.find(queryKey("logger"));
    auto itStart = theQuery.find(queryKey("start_time"));
    auto itEnd = theQuery.find(queryKey("end_time"));

    if(itLogger != theQuery.end() && itStart != theQuery.end() && itEnd != theQuery.end())
    {
        return GetLog(itLogger->second.Get(), itStart->second.Get(), itEnd->second.Get());
    }
    return theResponse;
}


time_t Server::GetDateTime(time_t date, const std::vector<std::string>& vLine)
{
    time_t timeMessage(0);
    std::tm atm= {};
    atm.tm_isdst = -1;

    if(vLine.empty() == false)
    {
        auto sTime = vLine[0];
        //remove the milliseconds
        auto nPos = sTime.find('.');
        if(nPos != std::string::npos)
        {
            sTime = sTime.substr(0,nPos);
        }

        std::stringstream ssDateTime;
        ssDateTime << std::put_time(localtime(&date), "%Y-%m-%d");
        ssDateTime << " " << sTime;
        ssDateTime >> std::get_time(&atm, "%Y-%m-%d %H:%M:%S");
        if(ssDateTime.fail())
        {
            pmlLog(pml::LOG_DEBUG) << "failed to parse date time: " << ssDateTime.str();
        }
        else
        {
            if(atm.tm_year < 100)
            {
                atm.tm_year += 100;
            }
            timeMessage = mktime(&atm);
        }
    }
    return timeMessage;
}

pml::restgoose::response Server::GetLog(const std::string& sLogger, const std::string& sStart, const std::string& sEnd)
{
    std::filesystem::path logDir = m_config.Get(jsonConsts::path,jsonConsts::log,".");
    logDir /= sLogger;

    if(std::filesystem::exists(logDir) == false)
    {
        return pml::restgoose::response(404, "Log not found: " + logDir.string());
    }

    try
    {
        unsigned long nTimeFrom = std::stoul(sStart);
        unsigned long nTimeTo = std::stoul(sEnd);

        std::stringstream ssLog;

        //get an array of the logs we need to open
        for(time_t nTime = (time_t)nTimeFrom; (nTime/3600) <= ((time_t)nTimeTo/3600); nTime+=3600)
        {
            std::stringstream ssDate;
            ssDate << std::put_time(localtime(&nTime), "%Y-%m-%dT%H.log");

            auto logFile = logDir;
            logFile /= ssDate.str();

            std::ifstream inFile;
            inFile.open(logFile,std::ios::in);
            if(!inFile.is_open())
            {
                ssLog << std::put_time(localtime(&nTime), "%Y-%m-%d") << " " << std::put_time(localtime(&nTime), "%H:00:00");
                ssLog <<  "\tERROR\t------ NO LOG FILE FOUND FOR THIS HOUR ------" << std::endl;
            }
            else
            {
                pmlLog(pml::LOG_DEBUG) << logFile << " opened";
                inFile.clear();
                std::string sLine;
                while(!inFile.eof())
                {
                    getline(inFile,sLine,'\n');
                    pmlLog(pml::LOG_DEBUG) << "Log\t" << sLine;
                    if(sLine.empty() == false)
                    {
                        auto vSplit = SplitString(sLine, '\t', 3);
                        if(vSplit.size() > 2)
                        {
                            auto timeMessage = GetDateTime(nTime, vSplit);

                            if(timeMessage >= nTimeFrom && timeMessage <= nTimeTo)
                            {
                                ssLog << std::put_time(localtime(&nTime), "%Y-%m-%d") << " ";
                                ssLog << vSplit[0] << "\t" << vSplit[1]  << "\t" << vSplit[2] << std::endl;
                            }
                        }
                    }
                }
                inFile.close();
            }
        }

        pml::restgoose::response resp(200);
        //resp.contentType = headerValue("plain/text");
        resp.jsonData = ssLog.str();
        //resp.data = textData(ssLog.str());
        return resp;
    }
    catch(std::exception& e)
    {
        pmlLog(pml::LOG_WARN) << "Could not convert start and end times " << sStart << " " << sEnd;
        return pml::restgoose::response(400, "Could not convert start and end times");
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
    ipAddress theSocket = peer;
    auto nPos = theSocket.Get().find(':');
    if(nPos != std::string::npos)
    {
        theSocket = ipAddress(theSocket.Get().substr(0, nPos));
    }
    pmlLog() << "Websocket connection request from " << theSocket;
    return DoAuthenticateToken(theUser.Get(), theSocket);
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

bool Server::AuthenticateToken(const std::string& sToken)
{
    return DoAuthenticateToken(sToken, m_server.GetCurrentPeer(false));
}

bool Server::DoAuthenticateToken(const std::string& sToken, const ipAddress& peer)
{
    pmlLog() << "AuthenticateToken " << peer << "=" << sToken;
    auto itToken = m_mTokens.find(peer);
    if(itToken != m_mTokens.end() && itToken->second->GetId() == sToken)
    {
        itToken->second->Accessed();
        pmlLog() << "AuthenticateToken succes";
        return true;
    }
    pmlLog(pml::LOG_WARN) << "AuthenticateToken failed";
    return false;
}

pml::restgoose::response Server::PostLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode == 200)
    {
        if(theResponse.jsonData.isMember(jsonConsts::username) && theResponse.jsonData.isMember(jsonConsts::password) && theResponse.jsonData[jsonConsts::password].asString().empty() == false)
        {
            if(m_config.Get("users", theResponse.jsonData[jsonConsts::username].asString(), "") == theResponse.jsonData[jsonConsts::password].asString())
            {
                auto pCookie = std::make_shared<SessionCookie>(userName(theResponse.jsonData[jsonConsts::username].asString()), m_server.GetCurrentPeer(false));

                auto [itToken, ins] = m_mTokens.insert(std::make_pair(m_server.GetCurrentPeer(false), pCookie));
                if(ins == false)
                {
                    itToken->second = pCookie;
                }

                pml::restgoose::response resp(200);
                resp.jsonData["token"] = pCookie->GetId();
                pmlLog() << "Login complete: ";
                return resp;
            }
        }
    }
    return pml::restgoose::response(401, "Username or password incorrect");
}

pml::restgoose::response Server::DeleteLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    m_mTokens.erase(m_server.GetCurrentPeer(false));
    return RedirectToLogin();
}

pml::restgoose::response Server::RedirectToLogin()
{
    pml::restgoose::response theResponse(302);
    theResponse.mHeaders = {{headerName("Location"), headerValue("/")}};
    return theResponse;
}
