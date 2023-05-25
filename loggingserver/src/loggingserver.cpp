#include "loggingserver.h"
#include "launcher.h"
#include "jsonconsts.h"
#include "log.h"
#include <iomanip>
#include <sys/wait.h>
#include "loggingserver_version.h"

using namespace std::placeholders;
using namespace pml;

const std::string LoggingServer::LOGGERS     = "loggers";
const std::string LoggingServer::SOURCES     = "sources";

const endpoint LoggingServer::EP_LOGGERS     = endpoint("/x-api/"+LOGGERS);
const endpoint LoggingServer::EP_SOURCES     = endpoint("/x-api/"+SOURCES);
const endpoint LoggingServer::EP_WS_LOGGERS  = endpoint("/x-api/ws/"+LOGGERS);




LoggingServer::LoggingServer() : Server("loggingserver")
{

}



void LoggingServer::Init()
{
    //add luauncher callbacks
    m_launcher.Init(GetIniManager(), std::bind(&LoggingServer::StatusCallback, this, _1,_2), std::bind(&LoggingServer::ExitCallback, this, _1,_2,_3));
    ReadDiscoveryConfig();
}

void LoggingServer::ReadDiscoveryConfig()
{
    if(m_discovery.Read(GetIniManager().Get(jsonConsts::path, "discovery","discovery.ini")) == false)
    {
        pmlLog(pml::LOG_ERROR) << "Unable to read discovery config file, won't be able to find stream sources";
    }
}

void LoggingServer::AddCustomEndpoints()
{

    GetServer().AddEndpoint(pml::restgoose::GET, EP_LOGGERS, std::bind(&LoggingServer::GetLoggers, this, _1,_2,_3,_4));
    GetServer().AddEndpoint(pml::restgoose::POST, EP_LOGGERS, std::bind(&LoggingServer::PostLogger, this, _1,_2,_3,_4));

    GetServer().AddEndpoint(pml::restgoose::GET, EP_SOURCES, std::bind(&LoggingServer::GetSources, this, _1,_2,_3,_4));

    GetServer().AddWebsocketEndpoint(EP_WS_LOGGERS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));

    //now add all the dynamic methodpoints
    for(const auto& [sName, pLauncher] : m_launcher.GetLaunchers())
    {
        AddLoggerEndpoints(sName);
    }

}

void LoggingServer::AddLoggerEndpoints(const std::string& sName)
{
    GetServer().AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&LoggingServer::GetLogger, this, _1,_2,_3,_4));
    GetServer().AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+STATUS), std::bind(&LoggingServer::GetLoggerStatus, this, _1,_2,_3,_4));
    GetServer().AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG), std::bind(&LoggingServer::GetLoggerConfig, this, _1,_2,_3,_4));
    GetServer().AddEndpoint(pml::restgoose::HTTP_DELETE, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&LoggingServer::DeleteLogger, this, _1,_2,_3,_4));

    GetServer().AddEndpoint(pml::restgoose::PATCH, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG), std::bind(&LoggingServer::PatchLoggerConfig, this, _1,_2,_3,_4));

    GetServer().AddEndpoint(pml::restgoose::PUT, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&LoggingServer::PutLoggerPower, this, _1,_2,_3,_4));

    GetServer().AddWebsocketEndpoint(endpoint(EP_WS_LOGGERS.Get()+"/"+sName), std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
}

void LoggingServer::DeleteCustomEndpoints()
{

    GetServer().DeleteEndpoint(pml::restgoose::GET, EP_LOGGERS);
    GetServer().DeleteEndpoint(pml::restgoose::POST, EP_LOGGERS);


    //now Delete all the dynamic methodpoints
    for(const auto& [sName, pLauncher] : m_launcher.GetLaunchers())
    {
        GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName));
        GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+STATUS));
        GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG));

        GetServer().DeleteEndpoint(pml::restgoose::HTTP_DELETE, endpoint(EP_LOGGERS.Get()+"/"+sName));
        GetServer().DeleteEndpoint(pml::restgoose::PUT, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+CONFIG));

    }
}


pml::restgoose::response LoggingServer::GetApi(const query&, const postData&, const endpoint&, const userName&) const
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetApi" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(LOGGERS);
    theResponse.jsonData.append(POWER);
    theResponse.jsonData.append(CONFIG);
    theResponse.jsonData.append(STATUS);
    theResponse.jsonData.append(INFO);
    theResponse.jsonData.append(SOURCES);
    theResponse.jsonData.append(UPDATE);
    return theResponse;
}

pml::restgoose::response LoggingServer::GetSources(const query&, const postData&, const endpoint&, const userName&) const
{
    pml::restgoose::response theResponse;
    theResponse.jsonData[jsonConsts::rtsp] = GetDiscoveredRtspSources();
    theResponse.jsonData[jsonConsts::sdp] = GetDiscoveredSdpSources();
    return theResponse;
}

Json::Value LoggingServer::GetDiscoveredRtspSources() const
{
    Json::Value jsArray(Json::arrayValue);

    if(iniManager inirtsp; inirtsp.Read(m_discovery.Get("rtsp", "file", "./rtsp.ini")))
    {
        auto pSection = inirtsp.GetSection("rtsp");
        if(pSection)
        {
            for(const auto& [key, value] : pSection->GetData())
            {
                jsArray.append(key);
            }
        }
        else
        {
            pmlLog(pml::LOG_WARN) << "Read rtsp ini file " << " but no rtsp section";
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN) << "Unable to read rtsp ini file ";
    }
    return jsArray;
}

Json::Value LoggingServer::GetDiscoveredSdpSources() const
{
    Json::Value jsArray(Json::arrayValue);

    for(const auto& entry : std::filesystem::directory_iterator(m_discovery.Get("sdp","path",".")))
    {
        jsArray.append(entry.path().stem().string());
    }
    return jsArray;
}

pml::restgoose::response LoggingServer::GetStatus(const query&, const postData&, const endpoint&, const userName&) const
{
    pml::restgoose::response theResponse(200);
    theResponse.jsonData = m_launcher.GetStatusSummary();
    return theResponse;
}

pml::restgoose::response LoggingServer::GetLoggers(const query&, const postData&, const endpoint&, const userName&) const
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

pml::restgoose::response LoggingServer::GetLogger(const query&, const postData&, const endpoint& theEndpoint, const userName&) const
{
    pml::restgoose::response theResponse(400);
    auto vPath = SplitString(theEndpoint.Get(),'/');

    if(m_launcher.GetLaunchers().find(vPath.back()) != m_launcher.GetLaunchers().end())
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

pml::restgoose::response LoggingServer::GetLoggerStatus(const query&, const postData&, const endpoint& theEndpoint, const userName&) const
{
    pmlLog() << "Server::GetLogger " << theEndpoint;
    pml::restgoose::response theResponse(400);
    auto vPath = SplitString(theEndpoint.Get(),'/');
    
    if(auto itLogger = m_launcher.GetLaunchers().find(vPath[vPath.size()-2]); itLogger != m_launcher.GetLaunchers().end())
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

pml::restgoose::response LoggingServer::GetLoggerConfig(const query&, const postData&, const endpoint& theEndpoint, const userName& ) const
{
    auto vPath = SplitString(theEndpoint.Get(),'/');
    return m_launcher.GetLoggerConfig(vPath[vPath.size()-2]);
}


pml::restgoose::response LoggingServer::PostLogger(const query&, const postData& vData, const endpoint&, const userName&)
{
    auto theData = ConvertPostDataToJson(vData);
    if(theData.nHttpCode == 200)
    {
        auto theResponse = m_launcher.AddLogger(theData);
        if(theResponse.nHttpCode == 200)
        {
            AddLoggerEndpoints(theData.jsonData[jsonConsts::name].asString());
        }
        return theResponse;
    }
    return theData;

}

pml::restgoose::response LoggingServer::DeleteLogger(const query&, const postData& vData, const endpoint& theEndpoint, const userName&)
{
    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode == 200)
    {
        if(theResponse.jsonData.isMember("password") == false)
        {
            return pml::restgoose::response(401, "No password sent");
        }
        if(theResponse.jsonData["password"].asString() != GetIniManager().Get(jsonConsts::api, jsonConsts::password, "2rnfgesgy8w!"))
        {
            return pml::restgoose::response(403, "Password is incorrect");
        }

        auto vUrl = SplitString(theEndpoint.Get(), '/');
        return m_launcher.RemoveLogger(vUrl.back());
    }
    return theResponse;
}

pml::restgoose::response LoggingServer::PatchLoggerConfig(const query&, const postData& vData, const endpoint& theEndpoint, const userName&)
{
    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode == 200)
    {
        auto vPath = SplitString(theEndpoint.Get(),'/');
        theResponse = m_launcher.UpdateLoggerConfig(vPath[vPath.size()-2], theResponse.jsonData);
    }

    return theResponse;
}

pml::restgoose::response LoggingServer::PutLoggerPower(const query&, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode == 200)
    {
        if(theResponse.jsonData.isMember("password") == false)
        {
            return pml::restgoose::response(401, "No password sent");
        }
        if(theResponse.jsonData["password"].asString() != GetIniManager().Get(jsonConsts::api, jsonConsts::password, "2rnfgesgy8w!"))
        {
            pmlLog(pml::LOG_DEBUG) << "Sent " << theResponse.jsonData["password"].asString() << " should be " << GetIniManager().Get(jsonConsts::api, jsonConsts::password, "2rnfgesgy8w!");
            return pml::restgoose::response(403, "Password is incorrect");
        }

        auto vPath = SplitString(theEndpoint.Get(),'/');
        return m_launcher.RestartLogger(vPath[vPath.size()-1]);
    }
    return theResponse;
}


Json::Value LoggingServer::GetVersion() const
{
    //get all the version numbers...
    Json::Value jsonData;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::rev] = pml::loggingserver::GIT_REV;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::tag] = pml::loggingserver::GIT_TAG;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::branch] = pml::loggingserver::GIT_BRANCH;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::date] = pml::loggingserver::GIT_DATE;
    jsonData[jsonConsts::server][jsonConsts::major] = pml::loggingserver::VERSION_MAJOR;
    jsonData[jsonConsts::server][jsonConsts::minor] = pml::loggingserver::VERSION_MINOR;
    jsonData[jsonConsts::server][jsonConsts::patch] = pml::loggingserver::VERSION_PATCH;
    jsonData[jsonConsts::server][jsonConsts::version] = pml::loggingserver::VERSION_STRING;
    jsonData[jsonConsts::server][jsonConsts::date] = pml::loggingserver::BUILD_TIME;

    auto js = ConvertToJson(Exec("logger -v"));
    if(js)
    {
        jsonData[jsonConsts::logger] = *js;
    }

    //get versions of other applications...
    return jsonData;
}

void LoggingServer::StatusCallback(const std::string& sLoggerId, const Json::Value& jsStatus)
{
    //lock as jsStatus can be called by pipe thread and server thread
    std::scoped_lock<std::mutex> lg(GetMutex());
    GetServer().SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLoggerId)}, jsStatus);
}

void LoggingServer::ExitCallback(const std::string& sLoggerId, int nExit, bool bRemove)
{
    //lock as jsStatus can be called by pipe thread and server thread
    std::scoped_lock<std::mutex> lg(GetMutex());

    auto jsStatus = Json::Value(Json::objectValue);    //reset

    jsStatus[jsonConsts::id] = sLoggerId;
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

    GetServer().SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLoggerId)}, jsStatus);


    if(bRemove)
    {
        GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sLoggerId));
        GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sLoggerId+"/"+STATUS));
        GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sLoggerId+"/"+CONFIG));
        GetServer().DeleteEndpoint(pml::restgoose::HTTP_DELETE, endpoint(EP_LOGGERS.Get()+"/"+sLoggerId));
        GetServer().DeleteEndpoint(pml::restgoose::PUT, endpoint(EP_LOGGERS.Get()+"/"+sLoggerId+"/"+CONFIG));
        GetServer().DeleteEndpoint(pml::restgoose::PUT, endpoint(EP_LOGGERS.Get()+"/"+sLoggerId));

        //@todo remove websocket endpoints
        //GetServer().AddWebsocketEndpoint(endpoint(EP_WS_LOGGERS.Get()+"/"+sName), std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
    }
}

Json::Value LoggingServer::GetCustomStatusSummary() const 
{
    return m_launcher.GetStatusSummary();
}
