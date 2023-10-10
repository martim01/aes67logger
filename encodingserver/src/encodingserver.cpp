#include "encodingserver.h"
#include "encodingserver_version.h"
#include "jsonconsts.h"
#include "log.h"
#include <sys/wait.h>

using namespace std::placeholders;
using namespace pml;

const std::string EncodingServer::ENCODERS    = "encoders";
const endpoint EncodingServer::EP_ENCODERS    = endpoint("x-api/"+ENCODERS);
const endpoint EncodingServer::EP_WS_ENCODERS = endpoint("x-api/ws/"+ENCODERS);


EncodingServer::EncodingServer() : Server("encodingserver")
{

}

void EncodingServer::Init()
{
    
    m_launcher.Init(GetIniManager(), std::bind(&EncodingServer::EncoderCallback, this, _1,_2,_3),
                                std::bind(&EncodingServer::StatusCallback, this, _1, _2),
                                std::bind(&EncodingServer::ExitCallback, this, _1, _2,_3));


}

void EncodingServer::AddCustomEndpoints()
{

    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "AddCustomEndpoints";

    GetServer().AddEndpoint(pml::restgoose::GET, endpoint("/x-api/encoders"), std::bind(&EncodingServer::GetEncoders, this, _1,_2,_3,_4));

    AddEncoderEndpoints();


    GetServer().AddWebsocketEndpoint(EP_WS_ENCODERS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&Server::WebsocketMessage, this, _1, _2), std::bind(&Server::WebsocketClosed, this, _1, _2));

}

pml::restgoose::response EncodingServer::GetStatus(const query&, const postData&, const endpoint&, const userName&) const
{
    pml::restgoose::response theResponse(200);
    theResponse.jsonData = m_launcher.GetStatusSummary();
    return theResponse;
}


void EncodingServer::DeleteCustomEndpoints()
{
    GetServer().DeleteEndpoint(pml::restgoose::GET, EP_ENCODERS);
    
}

pml::restgoose::response EncodingServer::GetApi(const query& , const postData& , const endpoint& , const userName& ) const
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetApi" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(ENCODERS);
    theResponse.jsonData.append(POWER);
    theResponse.jsonData.append(CONFIG);
    theResponse.jsonData.append(STATUS);
    theResponse.jsonData.append(INFO);
    theResponse.jsonData.append(UPDATE);
    return theResponse;
}


void EncodingServer::AddEncoderEndpoints()
{
    for(const auto& [name, pLauncher] : m_launcher.GetEncoders())
    {
        AddEncoderEndpoints(name);
    }
}

void EncodingServer::AddEncoderEndpoints(const std::string& sName)
{
    GetServer().AddEndpoint(pml::restgoose::GET, endpoint(EP_ENCODERS.Get()+"/"+sName), std::bind(&EncodingServer::GetEncoder, this, _1,_2,_3,_4));
    

    GetServer().AddWebsocketEndpoint(endpoint(EP_WS_ENCODERS.Get()+"/"+sName), std::bind(&EncodingServer::WebsocketAuthenticate, this, _1,_2, _3, _4), std::bind(&EncodingServer::WebsocketMessage, this, _1, _2), std::bind(&EncodingServer::WebsocketClosed, this, _1, _2));
}


void EncodingServer::RemoveEncoderEndpoints(const std::string& sName)
{
    GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_ENCODERS.Get()+"/"+sName));
    //@todo remove websocket endpoints....
}


pml::restgoose::response EncodingServer::GetEncoders(const query& , const postData& , const endpoint& , const userName& ) const
{
    pmlLog(pml::LOG_DEBUG) << "Endpoints\t" << "GetEncoders" ;
    
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    
    for(const auto& [name, obj] : m_launcher.GetEncoders())
    {
        theResponse.jsonData.append(name);
    }
    
    return theResponse;
}

pml::restgoose::response EncodingServer::GetEncoder(const query& , const postData& , const endpoint& theEndpoint, const userName& ) const
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
        return pml::restgoose::response(404, std::string("Logger not found"));
    }
}



void EncodingServer::StatusCallback(const std::string& sEncoderId, const Json::Value& jsStatus)
{
    //lock as jsStatus can be called by pipe thread and server thread
    std::scoped_lock<std::mutex> lg(GetMutex());
    GetServer().SendWebsocketMessage({endpoint(EP_WS_ENCODERS.Get()+"/"+sEncoderId), EP_WS_STATUS}, jsStatus);
}


void EncodingServer::ExitCallback(const std::string& sEncoderId, int nExit, bool bRemove)
{
    //lock as jsStatus can be called by pipe thread and server thread
    std::scoped_lock<std::mutex> lg(GetMutex());

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

    GetServer().SendWebsocketMessage({endpoint(EP_WS_ENCODERS.Get()+"/"+sEncoderId)}, jsStatus);


    if(bRemove)
    {
        GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_ENCODERS.Get()+"/"+sEncoderId));
        GetServer().DeleteEndpoint(pml::restgoose::GET, endpoint(EP_ENCODERS.Get()+"/"+sEncoderId+"/"+STATUS));

        //@todo remove websocket endpoints
    }
}



void EncodingServer::EncoderCallback(const std::string& sEncoderId, const std::string& sType, bool bAdded)
{
    std::scoped_lock<std::mutex> lg(GetMutex());
    if(bAdded)
    {
        EncoderCreated(sEncoderId);
    }
    else
    {
        EncoderDeleted(sEncoderId);
    }
}


void EncodingServer::EncoderCreated(const std::string& sEncoder)
{
    AddEncoderEndpoints(sEncoder);

    Json::Value jsValue;
    jsValue["field"] = "encoder";
    jsValue["action"] = "created";
    jsValue["encoder"] = sEncoder;

    GetServer().SendWebsocketMessage({EP_WS_ENCODERS}, jsValue);
}

void EncodingServer::EncoderDeleted(const std::string& sEncoder)
{
    Json::Value jsValue;
    jsValue["field"] = "encoder";
    jsValue["action"] = "deleted";
    jsValue["encoder"] = sEncoder;
    GetServer().SendWebsocketMessage({endpoint(EP_WS_ENCODERS.Get()+"/"+sEncoder)}, jsValue);
    RemoveEncoderEndpoints(sEncoder);
}





Json::Value EncodingServer::GetVersion() const
{
    //get all the version numbers...
    Json::Value jsonData;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::rev] = pml::encodingserver::GIT_REV;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::tag] = pml::encodingserver::GIT_TAG;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::branch] = pml::encodingserver::GIT_BRANCH;
    jsonData[jsonConsts::server][jsonConsts::git][jsonConsts::date] = pml::encodingserver::GIT_DATE;
    jsonData[jsonConsts::server][jsonConsts::major] = pml::encodingserver::VERSION_MAJOR;
    jsonData[jsonConsts::server][jsonConsts::minor] = pml::encodingserver::VERSION_MINOR;
    jsonData[jsonConsts::server][jsonConsts::patch] = pml::encodingserver::VERSION_PATCH;
    jsonData[jsonConsts::server][jsonConsts::version] = pml::encodingserver::VERSION_STRING;
    jsonData[jsonConsts::server][jsonConsts::date] = pml::encodingserver::BUILD_TIME;

    jsonData[jsonConsts::encoders] = m_launcher.GetEncoderVersions();
        
    return jsonData;
}



Json::Value EncodingServer::GetCustomStatusSummary() const 
{
    return m_launcher.GetStatusSummary();
}
