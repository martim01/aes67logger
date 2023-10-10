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

const std::string PlaybackServer::LOGGERS     = "loggers";
const std::string PlaybackServer::DOWNLOAD    = "download";

const endpoint PlaybackServer::EP_LOGGERS     = endpoint("/x-api/"+LOGGERS);
const endpoint PlaybackServer::EP_WS_LOGGERS  = endpoint("/x-api/ws/"+LOGGERS);



PlaybackServer::PlaybackServer() : Server("playbackserver")
{

}

void PlaybackServer::Init()
{
    m_pManager = std::make_unique<LoggerManager>(*this);
    m_pManager->EnumLoggers();
    m_pManager->WatchLoggerPath();
}

void PlaybackServer::AddCustomEndpoints()
{
    pmlLog() << "Create Custom Endpoints";

    GetServer().AddEndpoint(pml::restgoose::GET, EP_LOGGERS, std::bind(&PlaybackServer::GetLoggers, this, _1,_2,_3,_4));
    AddLoggerEndpoints();

    GetServer().AddWebsocketEndpoint(EP_WS_LOGGERS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&Server::WebsocketMessage, this, _1, _2), std::bind(&Server::WebsocketClosed, this, _1, _2));

}


void PlaybackServer::DeleteCustomEndpoints()
{
    GetServer().DeleteEndpoint(pml::restgoose::GET, EP_LOGGERS);   
}


pml::restgoose::response PlaybackServer::GetApi(const query& , const postData& , const endpoint& , const userName& ) const
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetApi" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(LOGGERS);
    return theResponse;
}

Json::Value PlaybackServer::GetVersion() const
{
     Json::Value jsonData;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::rev] = pml::playbackserver::GIT_REV;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::tag] = pml::playbackserver::GIT_TAG;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::branch] = pml::playbackserver::GIT_BRANCH;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::date] = pml::playbackserver::GIT_DATE;
    jsonData[jsonConsts::server][jsonConsts::major] = pml::playbackserver::VERSION_MAJOR;
    jsonData[jsonConsts::server][jsonConsts::minor] = pml::playbackserver::VERSION_MINOR;
    jsonData[jsonConsts::server][jsonConsts::patch] = pml::playbackserver::VERSION_PATCH;
    jsonData[jsonConsts::server][jsonConsts::version] = pml::playbackserver::VERSION_STRING;
    jsonData[jsonConsts::server][jsonConsts::date] = pml::playbackserver::BUILD_TIME;
    return jsonData;

}

Json::Value PlaybackServer::GetCustomStatusSummary() const
{
    return Json::Value(Json::objectValue);
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
    GetServer().AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName), std::bind(&PlaybackServer::GetLogger, this, _1,_2,_3,_4));
    
    for(const auto& [type, setFiles] : pLogger->GetEncodedFiles())
    {
       GetServer().AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+type), std::bind(&PlaybackServer::GetLoggerFiles, this, _1,_2,_3,_4), true);
       GetServer().AddEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+type+"/"+DOWNLOAD), std::bind(&PlaybackServer::DownloadLoggerFile, this, _1,_2,_3,_4), true);
    }

    GetServer().AddWebsocketEndpoint(endpoint(EP_WS_LOGGERS.Get()+"/"+sName), std::bind(&PlaybackServer::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&PlaybackServer::WebsocketMessage, this, _1, _2), std::bind(&PlaybackServer::WebsocketClosed, this, _1, _2));
}


void PlaybackServer::RemoveLoggerEndpoints(const std::string& sName, std::shared_ptr<LoggerObserver> pLogger)
{
    GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName));
    for(const auto& [type, setFiles] : pLogger->GetEncodedFiles())
    {
        GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_LOGGERS.Get()+"/"+sName+"/"+type));
    }
    //@todo remove websocket endpoints....
}


pml::restgoose::response PlaybackServer::GetLoggers(const query& , const postData& , const endpoint& , const userName& ) const
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetLoggers" ;
    
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    
    for(const auto& [name, obj] : m_pManager->GetLoggers())
    {
        theResponse.jsonData.append(name);
    }
    
    return theResponse;
}

pml::restgoose::response PlaybackServer::GetLogger(const query& , const postData& , const endpoint& theEndpoint, const userName& ) const
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
        return pml::restgoose::response(404, std::string("Logger not found"));
    }
}

pml::restgoose::response PlaybackServer::GetLoggerFiles(const query& , const postData& , const endpoint& theEndpoint, const userName& ) const
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
            for(const auto& path : itFiles->second)
            {
                theResponse.jsonData.append(path.stem().string());
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
        return pml::restgoose::response(404, std::string("Logger was not found"));
    }
}

pml::restgoose::response PlaybackServer::DownloadLoggerFile(const query& theQuery, const postData& , const endpoint& theEndpoint, const userName&)  const
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
        return pml::restgoose::response(404, std::string("Logger was not found"));
    }
}



void PlaybackServer::LoggerCreated(const std::string& sLogger, std::shared_ptr<LoggerObserver> pLogger)
{
    AddLoggerEndpoints(sLogger, pLogger);

    Json::Value jsValue;
    jsValue["field"] = "logger";
    jsValue["action"] = "created";
    jsValue["logger"] = sLogger;

    GetServer().SendWebsocketMessage({EP_WS_LOGGERS}, jsValue);
}

void PlaybackServer::LoggerDeleted(const std::string& sLogger, std::shared_ptr<LoggerObserver> pLogger)
{
    Json::Value jsValue;
    jsValue["field"] = "logger";
    jsValue["action"] = "deleted";
    jsValue["logger"] = sLogger;
    GetServer().SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLogger)}, jsValue);
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

    GetServer().SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLogger+"/"+path.extension().string())}, jsValue);
}

void PlaybackServer::FileDeleted(const std::string& sLogger, const std::filesystem::path& path)
{
    Json::Value jsValue;
    jsValue["field"] = "file";
    jsValue["action"] = "deleted";
    jsValue["logger"] = sLogger;
    jsValue["type"] = path.extension().string();
    jsValue["file"] = path.stem().string();

    GetServer().SendWebsocketMessage({endpoint(EP_WS_LOGGERS.Get()+"/"+sLogger+"/"+path.extension().string())}, jsValue);
}

pml::restgoose::response PlaybackServer::GetStatus(const query&, const postData&, const endpoint&, const userName&) const
{
    pml::restgoose::response theResponse(200);
    return theResponse;
}