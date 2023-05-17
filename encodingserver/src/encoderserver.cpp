#include "encoderserver.h"
#include "json/json.h"
#include <sstream>
#include "log.h"
#include "logtofile.h"
#include <chrono>
#include "aes67utils.h"
#include "jsonconsts.h"
#include <iomanip>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/reboot.h>

#include "encoderserver_version.h"

using namespace std::placeholders;
using namespace pml;

const std::string EncoderServer::ROOT        = "/";             //GET
const std::string EncoderServer::API         = "x-api";          //GET
const std::string EncoderServer::LOGIN       = "login";
const std::string EncoderServer::ENCODERS    = "encoders";
const std::string EncoderServer::STATUS      = "status";
const std::string EncoderServer::WS          = "ws";

const endpoint EncoderServer::EP_ROOT        = endpoint("");
const endpoint EncoderServer::EP_API         = endpoint(ROOT+API);
const endpoint EncoderServer::EP_LOGIN       = endpoint(EP_API.Get()+"/"+LOGIN);
const endpoint EncoderServer::EP_ENCODERS    = endpoint(EP_API.Get()+"/"+ENCODERS);
const endpoint EncoderServer::EP_WS          = endpoint(EP_API.Get()+"/"+WS);
const endpoint EncoderServer::EP_WS_ENCODERS = endpoint(EP_API.Get()+"/"+WS+"/"+ENCODERS);


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


EncoderServer::EncoderServer()=default;



void EncoderServer::InitLogging()
{
    if(m_config.Get("logging", "console", 0L) > -1 )
    {
        if(m_nLogToConsole == -1)
        {
            m_nLogToConsole = pml::LogStream::AddOutput(std::make_unique<pml::LogOutput>());
        }
        pml::LogStream::SetOutputLevel(m_nLogToConsole, pml::enumLevel(m_config.Get("logging", "console",(long) pml::LOG_TRACE)));
    }
    else if(m_nLogToConsole != -1)
    {
        pml::LogStream::RemoveOutput(m_nLogToConsole);
        m_nLogToConsole = -1;
    }

    if(m_config.Get("logging", "file", (long)pml::LOG_INFO) > -1)
    {
        if(m_nLogToFile == -1)
        {
            std::filesystem::path pathLog = m_config.Get(jsonConsts::path,jsonConsts::log,".");
            pathLog /= "loggermanager";
            m_nLogToFile = pml::LogStream::AddOutput(std::make_unique<pml::LogToFile>(pathLog));
        }
        pml::LogStream::SetOutputLevel(m_nLogToFile, pml::enumLevel(m_config.Get("logging", "file", (long)pml::LOG_INFO)));
    }
    else if(m_nLogToFile != -1)
    {
        pml::LogStream::RemoveOutput(m_nLogToFile);
        m_nLogToFile = -1;
    }

}

int EncoderServer::Run(const std::string& sConfigFile)
{
    if(m_config.Read(sConfigFile) == false)
    {
        pml::LogStream::AddOutput(std::make_unique<pml::LogOutput>());
        pmlLog(pml::LOG_CRITICAL) << "Could not open '" << sConfigFile << "' exiting.";
        return -1;
    }

    InitLogging();

    pmlLog() << "Core\tStart" ;

    auto addr = ipAddress(GetIpAddress(m_config.Get(jsonConsts::api, jsonConsts::interface, "eth0")));

    if(m_server.Init(fileLocation(m_config.Get(jsonConsts::api, "sslCert", "")), fileLocation(m_config.Get(jsonConsts::api, "sslKey", "")), 
    addr, m_config.Get(jsonConsts::api, "port", 8080L), EP_API, true,true))
    {

        m_server.SetAuthorizationTypeBearer(std::bind(&EncoderServer::AuthenticateToken, this, _1), std::bind(&EncoderServer::RedirectToLogin, this), true);
        m_server.SetUnprotectedEndpoints({methodpoint(pml::restgoose::GET, endpoint("")),
                                          methodpoint(pml::restgoose::GET, endpoint("/index.html")),
                                          methodpoint(pml::restgoose::POST, EP_LOGIN),
                                          methodpoint(pml::restgoose::GET, endpoint("/js/*")),
                                          methodpoint(pml::restgoose::GET, endpoint("/uikit/*")),
                                          methodpoint(pml::restgoose::GET, endpoint("/images/*"))});

        m_server.SetStaticDirectory(m_config.Get(jsonConsts::api,jsonConsts::static_pages, "/var/www"));

        m_launcher.Init(m_config, std::bind(&EncoderServer::EncoderCallback, this, _1,_2,_3),
                                  std::bind(&EncoderServer::StatusCallback, this, _1, _2),
                                  std::bind(&EncoderServer::ExitCallback, this, _1, _2,_3));


        //add server callbacks
        CreateEndpoints();
        
        AddEncoderEndpoints();

        //start the server loop
        m_server.Run(false, std::chrono::milliseconds(50));

        pmlLog() << "Core\tStop" ;
        DeleteEndpoints();

        return 0;
    }
    return -2;
}

bool EncoderServer::CreateEndpoints()
{

    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "CreateEndpoints" ;

    m_server.AddEndpoint(pml::restgoose::POST, EP_LOGIN, std::bind(&EncoderServer::PostLogin, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::HTTP_DELETE, EP_LOGIN, std::bind(&EncoderServer::DeleteLogin, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::GET, EP_API, std::bind(&EncoderServer::GetApi, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_ENCODERS, std::bind(&EncoderServer::GetEncoders, this, _1,_2,_3,_4));

    m_server.AddWebsocketEndpoint(EP_WS, std::bind(&EncoderServer::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&EncoderServer::WebsocketMessage, this, _1, _2), std::bind(&EncoderServer::WebsocketClosed, this, _1, _2));

    m_server.AddWebsocketEndpoint(EP_WS_ENCODERS, std::bind(&EncoderServer::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&EncoderServer::WebsocketMessage, this, _1, _2), std::bind(&EncoderServer::WebsocketClosed, this, _1, _2));


    //Add the loop callback function
    m_server.SetLoopCallback(std::bind(&EncoderServer::LoopCallback, this, _1));

    return true;
}


void EncoderServer::DeleteEndpoints()
{

    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "DeleteEndpoints" ;

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_API);


    m_server.DeleteEndpoint(pml::restgoose::GET, EP_ENCODERS);

    
}

pml::restgoose::response EncoderServer::GetRoot(const query& , const postData& , const endpoint& , const userName& ) const
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetRoot" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(API);
    return theResponse;
}

pml::restgoose::response EncoderServer::GetApi(const query& , const postData& , const endpoint& , const userName& ) const
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetApi" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(ENCODERS);
    return theResponse;
}


void EncoderServer::AddEncoderEndpoints()
{
    for(const auto& [name, pLauncher] : m_launcher.GetEncoders())
    {
        AddEncoderEndpoints(name);
    }
}

void EncoderServer::AddEncoderEndpoints(const std::string& sName)
{
    m_server.AddEndpoint(pml::restgoose::GET, endpoint(EP_ENCODERS.Get()+"/"+sName), std::bind(&EncoderServer::GetEncoder, this, _1,_2,_3,_4));
    

    m_server.AddWebsocketEndpoint(endpoint(EP_WS_ENCODERS.Get()+"/"+sName), std::bind(&EncoderServer::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&EncoderServer::WebsocketMessage, this, _1, _2), std::bind(&EncoderServer::WebsocketClosed, this, _1, _2));
}


void EncoderServer::RemoveEncoderEndpoints(const std::string& sName)
{
    m_server.DeleteEndpoint(pml::restgoose::GET, endpoint(EP_ENCODERS.Get()+"/"+sName));
    //@todo remove websocket endpoints....
}


pml::restgoose::response EncoderServer::GetEncoders(const query& , const postData& , const endpoint& , const userName& ) const
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetEncoders" ;
    
    pml::restgoose::response theResponse;
    for(const auto& [name, obj] : m_launcher.GetEncoders())
    {
        theResponse.jsonData.append(name);
    }
    
    return theResponse;
}

pml::restgoose::response EncoderServer::GetEncoder(const query& , const postData& , const endpoint& theEndpoint, const userName& ) const
{
    auto vPath = SplitString(theEndpoint.Get(),'/');
    
    auto itLogger = m_launcher.GetEncoders().find(vPath.back());
    if(itLogger != m_launcher.GetEncoders().end())
    {
        pml::restgoose::response theResponse(200);
        return theResponse;
    }
    else
    {
        return pml::restgoose::response(404, "Logger not found");
    }
}


time_t EncoderServer::GetDateTime(time_t date, const std::vector<std::string>& vLine) const
{
    time_t timeMessage(0);
    std::tm atm= {};
    atm.tm_isdst = -1;

    if(vLine.empty() == false)
    {
        auto sTime = vLine[0];
        //remove the milliseconds
        if(auto nPos = sTime.find('.'); nPos != std::string::npos)
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

void EncoderServer::StatusCallback(const std::string& sEncoderId, const Json::Value& jsStatus)
{
    //lock as jsStatus can be called by pipe thread and server thread
    std::scoped_lock<std::mutex> lg(m_mutex);
    m_server.SendWebsocketMessage({endpoint(EP_WS_ENCODERS.Get()+"/"+sEncoderId)}, jsStatus);
}


void EncoderServer::ExitCallback(const std::string& sEncoderId, int nExit, bool bRemove)
{
    //lock as jsStatus can be called by pipe thread and server thread
    std::scoped_lock<std::mutex> lg(m_mutex);

    auto jsStatus = Json::Value(Json::objectValue);    //reset

    jsStatus[jsonConsts::id] = sEncoderId;
    jsStatus[jsonConsts::status] = jsonConsts::stopped;

    if(WIFEXITED(nExit))
    {
        jsStatus[jsonConsts::exit][jsonConsts::code] = WEXITSTATUS(nExit);
        pmlLog() << "Logger exited " << "Code: " << WEXITSTATUS(nExit);
    }
    if(WIFSIGNALED(nExit))
    {
        jsStatus[jsonConsts::exit][jsonConsts::signal][jsonConsts::code] = WTERMSIG(nExit);
        jsStatus[jsonConsts::exit][jsonConsts::signal][jsonConsts::description] = strsignal(WTERMSIG(nExit));
        pmlLog() << "Logger signaled "  << "Code: " << WTERMSIG(nExit) << " " << strsignal(WTERMSIG(nExit));

        if(WCOREDUMP(nExit))
        {
            jsStatus[jsonConsts::exit][jsonConsts::core_dump] = true;
        }
    }
    if(WIFSTOPPED(nExit))
    {
        jsStatus[jsonConsts::stopped][jsonConsts::signal] = WSTOPSIG(nExit);
        pmlLog() << "Logger stopped "  << "Signal: " << WSTOPSIG(nExit);
    }
    if(WIFCONTINUED(nExit))
    {
        jsStatus[jsonConsts::resumed][jsonConsts::signal] = WSTOPSIG(nExit);
        pmlLog() << "Logger resumed Signal:" << WSTOPSIG(nExit);
    }

    m_server.SendWebsocketMessage({endpoint(EP_WS_ENCODERS.Get()+"/"+sEncoderId)}, jsStatus);


    if(bRemove)
    {
        m_server.DeleteEndpoint(pml::restgoose::GET, endpoint(EP_ENCODERS.Get()+"/"+sEncoderId));
        m_server.DeleteEndpoint(pml::restgoose::GET, endpoint(EP_ENCODERS.Get()+"/"+sEncoderId+"/"+STATUS));

        //@todo remove websocket endpoints
    }
}



void EncoderServer::LoopCallback(std::chrono::milliseconds durationSince)
{
    m_nTimeSinceLastCall += durationSince.count();


    if(m_nTimeSinceLastCall > 2000)
    {
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



bool EncoderServer::WebsocketAuthenticate(const endpoint&, const query&, const userName& theUser, const ipAddress& peer)
{
    ipAddress theSocket = peer;

    if(auto nPos = theSocket.Get().find(':'); nPos != std::string::npos)
    {
        theSocket = ipAddress(theSocket.Get().substr(0, nPos));
    }
    pmlLog() << "Websocket connection request from " << theSocket;
    return DoAuthenticateToken(theUser.Get(), theSocket);
}

bool EncoderServer::WebsocketMessage(const endpoint&, const Json::Value& jsData) const
{
    pmlLog() << "Websocket message '" << jsData << "'";
    return true;
}

void EncoderServer::WebsocketClosed(const endpoint&, const ipAddress& peer) const
{
    pmlLog() << "Websocket closed from " << peer;
}

bool EncoderServer::AuthenticateToken(const std::string& sToken)
{
    return DoAuthenticateToken(sToken, m_server.GetCurrentPeer(false));
}

bool EncoderServer::DoAuthenticateToken(const std::string& sToken, const ipAddress& peer)
{
    pmlLog() << "AuthenticateToken " << peer << "=" << sToken;
    if(auto itToken = m_mTokens.find(peer); itToken != m_mTokens.end() && itToken->second->GetId() == sToken)
    {
        itToken->second->Accessed();
        pmlLog() << "AuthenticateToken succes";
        return true;
    }
    pmlLog(pml::LOG_WARN) << "AuthenticateToken failed";
    return false;
}

pml::restgoose::response EncoderServer::PostLogin(const query&, const postData& vData, const endpoint&, const userName&)
{
    if(auto theResponse = ConvertPostDataToJson(vData); 
       theResponse.nHttpCode == 200 && theResponse.jsonData.isMember(jsonConsts::username) && 
       theResponse.jsonData.isMember(jsonConsts::password) && theResponse.jsonData[jsonConsts::password].asString().empty() == false && 
       m_config.Get("users", theResponse.jsonData[jsonConsts::username].asString(), "") == theResponse.jsonData[jsonConsts::password].asString())
    {
        auto pCookie = std::make_shared<SessionCookie>(userName(theResponse.jsonData[jsonConsts::username].asString()), m_server.GetCurrentPeer(false));

        auto [itToken, ins] = m_mTokens.try_emplace(m_server.GetCurrentPeer(false), pCookie);
        if(ins == false)
        {
            itToken->second = pCookie;
        }

        pml::restgoose::response resp(200);
        resp.jsonData["token"] = pCookie->GetId();
        pmlLog() << "Login complete: ";
        return resp;
    }
    return pml::restgoose::response(401, "Username or password incorrect");
}

pml::restgoose::response EncoderServer::DeleteLogin(const query&, const postData&, const endpoint&, const userName&)
{
    m_mTokens.erase(m_server.GetCurrentPeer(false));
    return RedirectToLogin();
}

pml::restgoose::response EncoderServer::RedirectToLogin() const
{
    pml::restgoose::response theResponse(302);
    theResponse.mHeaders = {{headerName("Location"), headerValue("/")}};
    return theResponse;
}

void EncoderServer::EncoderCallback(const std::string& sEncoderId, const std::string& sType, bool bAdded)
{
    std::scoped_lock<std::mutex> lg(m_mutex);
    if(bAdded)
    {
        EncoderCreated(sEncoderId);
    }
    else
    {
        EncoderDeleted(sEncoderId);
    }
}


void EncoderServer::EncoderCreated(const std::string& sEncoder)
{
    AddEncoderEndpoints(sEncoder);

    Json::Value jsValue;
    jsValue["field"] = "encoder";
    jsValue["action"] = "created";
    jsValue["encoder"] = sEncoder;

    m_server.SendWebsocketMessage({EP_WS_ENCODERS}, jsValue);
}

void EncoderServer::EncoderDeleted(const std::string& sEncoder)
{
    Json::Value jsValue;
    jsValue["field"] = "encoder";
    jsValue["action"] = "deleted";
    jsValue["encoder"] = sEncoder;
    m_server.SendWebsocketMessage({endpoint(EP_WS_ENCODERS.Get()+"/"+sEncoder)}, jsValue);
    RemoveEncoderEndpoints(sEncoder);
}
