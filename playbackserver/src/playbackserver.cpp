#include "playbackserver.h"
#include "json/json.h"
#include <sstream>
#include "log.h"
#include "logtofile.h"
#include <chrono>
#include "aes67utils.h"
#include "jsonconsts.h"
#include <iomanip>
#include "loggerobserver.h"
#include "playbackserver_version.h"

using namespace std::placeholders;
using namespace pml;

const std::string PlaybackServer::ROOT        = "/";             //GET
const std::string PlaybackServer::API         = "x-api";          //GET
const std::string PlaybackServer::LOGIN       = "login";
const std::string PlaybackServer::LOGGERS     = "loggers";
const std::string PlaybackServer::DOWNLOAD    = "download";
const std::string PlaybackServer::WS          = "ws";

const endpoint PlaybackServer::EP_ROOT        = endpoint("");
const endpoint PlaybackServer::EP_API         = endpoint(ROOT+API);
const endpoint PlaybackServer::EP_LOGIN       = endpoint(EP_API.Get()+"/"+LOGIN);
const endpoint PlaybackServer::EP_LOGGERS     = endpoint(EP_API.Get()+"/"+LOGGERS);
const endpoint PlaybackServer::EP_WS          = endpoint(EP_API.Get()+"/"+WS);
const endpoint PlaybackServer::EP_WS_LOGGERS  = endpoint(EP_API.Get()+"/"+WS+"/"+LOGGERS);


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


PlaybackServer::PlaybackServer() :
   m_nTimeSinceLastCall(0),
   m_nLogToConsole(-1),
   m_nLogToFile(-1),
   m_bLoggedThisHour(false)
{
}



void PlaybackServer::InitLogging()
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

int PlaybackServer::Run(const std::string& sConfigFile)
{
    if(m_config.Read(sConfigFile) == false)
    {
        pmlLog().AddOutput(std::unique_ptr<pml::LogOutput>(new pml::LogOutput()));
        pmlLog(pml::LOG_CRITICAL) << "Could not open '" << sConfigFile << "' exiting.";
        return -1;
    }

    InitLogging();

    pmlLog() << "Core\tStart" ;

    m_pManager = std::make_unique<LoggerManager>(*this);
    m_pManager->EnumLoggers();
    m_pManager->WatchLoggerPath();


    auto addr = ipAddress(GetIpAddress(m_config.Get(jsonConsts::api, jsonConsts::interface, "eth0")));

    if(m_server.Init(fileLocation(m_config.Get(jsonConsts::api, "sslCert", "")), fileLocation(m_config.Get(jsonConsts::api, "sslKey", "")), addr, m_config.Get(jsonConsts::api, "port", 8080), EP_API, true,true))
    {

        m_server.SetAuthorizationTypeBearer(std::bind(&PlaybackServer::AuthenticateToken, this, _1), std::bind(&PlaybackServer::RedirectToLogin, this), true);
        m_server.SetUnprotectedEndpoints({methodpoint(pml::restgoose::GET, endpoint("")),
                                          methodpoint(pml::restgoose::GET, endpoint("/index.html")),
                                          methodpoint(pml::restgoose::POST, EP_LOGIN),
                                          methodpoint(pml::restgoose::GET, endpoint("/js/*")),
                                          methodpoint(pml::restgoose::GET, endpoint("/uikit/*")),
                                          methodpoint(pml::restgoose::GET, endpoint("/images/*"))});

        m_server.SetStaticDirectory(m_config.Get(jsonConsts::api,jsonConsts::static_pages, "/var/www"));


        //add server callbacks
        CreateEndpoints();
        
        AddLoggerEndpoints();

        //start the server loop
        m_server.Run(false, std::chrono::milliseconds(50));

        pmlLog() << "Core\tStop" ;
        DeleteEndpoints();

        return 0;
    }
    return -2;
}

bool PlaybackServer::CreateEndpoints()
{

    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "CreateEndpoints" ;

    m_server.AddEndpoint(pml::restgoose::POST, EP_LOGIN, std::bind(&PlaybackServer::PostLogin, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::HTTP_DELETE, EP_LOGIN, std::bind(&PlaybackServer::DeleteLogin, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::GET, EP_API, std::bind(&PlaybackServer::GetApi, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_LOGGERS, std::bind(&PlaybackServer::GetLoggers, this, _1,_2,_3,_4));

    m_server.AddWebsocketEndpoint(EP_WS, std::bind(&PlaybackServer::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&PlaybackServer::WebsocketMessage, this, _1, _2), std::bind(&PlaybackServer::WebsocketClosed, this, _1, _2));

    m_server.AddWebsocketEndpoint(EP_WS_LOGGERS, std::bind(&PlaybackServer::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&PlaybackServer::WebsocketMessage, this, _1, _2), std::bind(&PlaybackServer::WebsocketClosed, this, _1, _2));


    //Add the loop callback function
    m_server.SetLoopCallback(std::bind(&PlaybackServer::LoopCallback, this, _1));

    return true;
}


void PlaybackServer::DeleteEndpoints()
{

    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "DeleteEndpoints" ;

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_API);


    m_server.DeleteEndpoint(pml::restgoose::GET, EP_LOGGERS);

    
}

pml::restgoose::response PlaybackServer::GetRoot(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetRoot" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(API);
    return theResponse;
}

pml::restgoose::response PlaybackServer::GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetApi" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(LOGGERS);
    return theResponse;
}


void PlaybackServer::AddLoggerEndpoints()
{
    for(const auto& [name, pLogger] : m_pManager->GetLoggers())
    {
        AddLoggerEndpoints(name, pLogger);
    }
}

void PlaybackServer::AddLoggerEndpoints(const std::string& sName, std::shared_ptr<LoggerObserver> pLogger)
{
    m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&PlaybackServer::GetLogger, this, _1,_2,_3,_4));
    
    for(const auto& [type, setFiles] : pLogger->GetEncodedFiles())
    {
       m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+type), std::bind(&PlaybackServer::GetLoggerFiles, this, _1,_2,_3,_4));
       m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+type+"/"+DOWNLOAD), std::bind(&PlaybackServer::DownloadLoggerFile, this, _1,_2,_3,_4));
    }

    m_server.AddWebsocketEndpoint(endpoint(EP_WS_LOGGERS.Get()+"/"+sName), std::bind(&PlaybackServer::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&PlaybackServer::WebsocketMessage, this, _1, _2), std::bind(&PlaybackServer::WebsocketClosed, this, _1, _2));
}


void PlaybackServer::RemoveLoggerEndpoints(const std::string& sName, std::shared_ptr<LoggerObserver> pLogger)
{
    m_server.DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName));
    for(const auto& [type, setFiles] : pLogger->GetEncodedFiles())
    {
        m_server.DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+type));
    }
    //@todo remove websocket endpoints....
}


pml::restgoose::response PlaybackServer::GetLoggers(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetLoggers" ;
    
    pml::restgoose::response theResponse;
    for(const auto& [name, obj] : m_pManager->GetLoggers())
    {
        theResponse.jsonData.append(name);
    }
    
    return theResponse;
}

pml::restgoose::response PlaybackServer::GetLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    auto vPath = SplitString(theEndpoint.Get(),'/');
    
    auto itLogger = m_pManager->GetLoggers().find(vPath.back());
    if(itLogger != m_pManager->GetLoggers().end())
    {
        pml::restgoose::response theResponse(200);
        for(const auto& [type, setFiles] : itLogger->second->GetEncodedFiles())
        {
            theResponse.jsonData.append(type);
        }
        return theResponse;
    }
    else
    {
        return pml::restgoose::response(404, "Logger not found");
    }
}

pml::restgoose::response PlaybackServer::GetLoggerFiles(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{

    auto vPath = SplitString(theEndpoint.Get(),'/');
    
    auto itLogger = m_pManager->GetLoggers().find(vPath[vPath.size()-2]);
    pmlLog() << "GetLoggerFiles: " << vPath[vPath.size()-2] << ": " << vPath.back();

    if(itLogger != m_pManager->GetLoggers().end())
    {
        pml::restgoose::response theResponse(200);
        auto itFiles = itLogger->second->GetEncodedFiles().find(vPath.back());
        if(itFiles != itLogger->second->GetEncodedFiles().end() && itFiles->second.empty() == false)
        {
//	    for(const auto& path : itFiles->second)
//	    {
//		theResponse.jsonData["got"].append(path.string());
//		}
//	return theResponse;
            std::string sFormat = "%Y-%m-%dT%H-%M";

            auto start = ConvertStringToTimePoint((*itFiles->second.begin()).stem().string(), sFormat);
            auto end = ConvertStringToTimePoint((*itFiles->second.rbegin()).stem().string(), sFormat);
            
            if(start && end)
            {
                bool bInRange = false;
                Json::Value jsRange;
                for(auto tp = (*start); tp <= (*end); tp += std::chrono::minutes(1))
                {
                    auto filePath = (*itFiles->second.begin()).parent_path();
                    filePath /= ConvertTimeToString(tp, sFormat);
		    filePath.replace_extension(vPath.back());
//		    theResponse.jsonData["want"].append(filePath.string());
                    auto itFile = itFiles->second.find(filePath);
                    if(itFile != itFiles->second.end())
                    {
                        jsRange["end"] = (*itFile).stem().string();
                        if(!bInRange)
                        {
                            jsRange["start"] = (*itFile).stem().string();
                            bInRange = true;
                        }
                    }
                    else if(bInRange && itFile == itFiles->second.end())
                    {
                        theResponse.jsonData.append(jsRange);
                        bInRange = false;
                    }
                }
                if(bInRange)
                {
                    theResponse.jsonData.append(jsRange);
                    bInRange = false;
                }
            }
            return theResponse;
        }
        else
        {
            return pml::restgoose::response(404, "Logger has no files of that type "+vPath.back());
        }
    }
    else
    {
        return pml::restgoose::response(404, "Logger was not found");
    }
}

pml::restgoose::response PlaybackServer::DownloadLoggerFile(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    auto vPath = SplitString(theEndpoint.Get(),'/');
    
    auto itLogger = m_pManager->GetLoggers().find(vPath[vPath.size()-2]);
    pmlLog() << "DownloadLoggerFile: " << vPath[vPath.size()-2] << ": " << vPath.back();

    if(itLogger != m_pManager->GetLoggers().end())
    {
        return itLogger->second->CreateDownloadFile(vPath.back(), theQuery);
    }
    else
    {
        return pml::restgoose::response(404, "Logger was not found");
    }
}


time_t PlaybackServer::GetDateTime(time_t date, const std::vector<std::string>& vLine)
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



void PlaybackServer::LoopCallback(std::chrono::milliseconds durationSince)
{
    m_nTimeSinceLastCall += durationSince.count();


    if(m_nTimeSinceLastCall > 2000)
    {
//        m_server.SendWebsocketMessage({EP_WS_INFO}, m_info.GetInfo());

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



bool PlaybackServer::WebsocketAuthenticate(const endpoint& theEndpoint, const query& theQuery, const userName& theUser, const ipAddress& peer)
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

bool PlaybackServer::WebsocketMessage(const endpoint& theEndpoint, const Json::Value& jsData)
{
    pmlLog() << "Websocket message '" << jsData << "'";
    return true;
}

void PlaybackServer::WebsocketClosed(const endpoint& theEndpoint, const ipAddress& peer)
{
    pmlLog() << "Websocket closed from " << peer;
}

bool PlaybackServer::AuthenticateToken(const std::string& sToken)
{
    return DoAuthenticateToken(sToken, m_server.GetCurrentPeer(false));
}

bool PlaybackServer::DoAuthenticateToken(const std::string& sToken, const ipAddress& peer)
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

pml::restgoose::response PlaybackServer::PostLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
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

pml::restgoose::response PlaybackServer::DeleteLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    m_mTokens.erase(m_server.GetCurrentPeer(false));
    return RedirectToLogin();
}

pml::restgoose::response PlaybackServer::RedirectToLogin()
{
    pml::restgoose::response theResponse(302);
    theResponse.mHeaders = {{headerName("Location"), headerValue("/")}};
    return theResponse;
}


void PlaybackServer::LoggerCreated(const std::string& sLogger, std::shared_ptr<LoggerObserver> pLogger)
{
    AddLoggerEndpoints(sLogger, pLogger);

    Json::Value jsValue;
    jsValue["field"] = "logger";
    jsValue["action"] = "created";
    jsValue["logger"] = sLogger;

    m_server.SendWebsocketMessage({EP_WS_LOGGERS}, jsValue);
}

void PlaybackServer::LoggerDeleted(const std::string& sLogger, std::shared_ptr<LoggerObserver> pLogger)
{
    Json::Value jsValue;
    jsValue["field"] = "logger";
    jsValue["action"] = "deleted";
    jsValue["logger"] = sLogger;
    m_server.SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLogger)}, jsValue);
    RemoveLoggerEndpoints(sLogger, pLogger);
}

void PlaybackServer::FileCreated(const std::string& sLogger, const std::filesystem::path& path)
{
    Json::Value jsValue;
    jsValue["field"] = "file";
    jsValue["action"] = "created";
    jsValue["logger"] = sLogger;
    jsValue["type"] = path.extension().string();
    jsValue["file"] = path.stem().string();

    m_server.SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLogger+"/"+path.extension().string())}, jsValue);
}

void PlaybackServer::FileDeleted(const std::string& sLogger, const std::filesystem::path& path)
{
    Json::Value jsValue;
    jsValue["field"] = "file";
    jsValue["action"] = "deleted";
    jsValue["logger"] = sLogger;
    jsValue["type"] = path.extension().string();
    jsValue["file"] = path.stem().string();

    m_server.SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLogger+"/"+path.extension().string())}, jsValue);
}
