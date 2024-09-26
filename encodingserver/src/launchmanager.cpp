#include "launchmanager.h"
#include "launcher.h"
#include "log.h"
#include "inimanager.h"
#include "jsonconsts.h"
#include "aes67utils.h"
#include "inisection.h"
#include "httpclient.h"
#include "jwt-cpp/jwt.h"


using namespace std::placeholders;

std::string CreateJwt(const std::string& sSecret)
{
    auto token = jwt::create().set_issuer("vam")
                .set_id("{encodingserver}")
                .set_issued_at(std::chrono::system_clock::now())
                .set_expires_at(std::chrono::system_clock::now()+std::chrono::seconds(20))
                .sign(jwt::algorithm::hs256{sSecret});

    return token;
}

const std::string LaunchManager::LOG_PREFIX = "LaunchManager";

LaunchManager::LaunchManager() :
  m_client(std::bind(&LaunchManager::WebsocketConnection, this, _1, _2, _3), std::bind(&LaunchManager::WebsocketMessage, this, _1, _2))
{

}

LaunchManager::~LaunchManager()
{
    if(m_pThread)
    {
        m_context.stop();
        m_pThread->join();
    }
    //Encoders will shutdown in the launcher destructor
}

void LaunchManager::Init(const iniManager& iniConfig, const std::function<void(const std::string&, const std::string&, bool bAdded)>& encoderCallback, 
                                                      const std::function<void(const std::string&, const Json::Value&)>& statusCallback, 
                                                      const std::function<void(const std::string&, int, bool)>& exitCallback)
{
    m_sPathSockets = iniConfig.Get(jsonConsts::path, jsonConsts::sockets, "/var/local/loggers/sockets");
    m_sPathEncoded = iniConfig.Get(jsonConsts::path, jsonConsts::encoded, "/var/local/vam/audio");
    
    m_sVamUrl = iniConfig.Get(jsonConsts::path, "vam", "127.0.0.1:4431");
    m_sSecret = iniConfig.Get(jsonConsts::restricted_authentication, jsonConsts::secret, std::string());

    pmlLog() << "Secret='"<<m_sSecret<<"'";

    if(auto pSection = iniConfig.GetSection(jsonConsts::encoders); pSection)
    {
        for(const auto& [sType, sPath] : pSection->GetData())
        {
            m_mEncoderApps.try_emplace(sType, sPath);
        }
    }

    m_statusCallback = statusCallback;
    m_exitCallback = exitCallback;
    m_encoderCallback = encoderCallback;
 
    GetWavPath();
    EnumLoggers();
    LaunchAll();


    m_client.Run();
    m_client.Connect(endpoint(m_sWsProtocol+"://"+m_sVamUrl+"/x-api/ws/matrix?access_token="+CreateJwt(m_sSecret)));
    
}

void LaunchManager::GetWavPath()
{   
    auto client = pml::restgoose::HttpClient(pml::restgoose::GET, endpoint(m_sHttpProtocol+"://"+m_sVamUrl+"/x-api/builders/destinations/Recorder"));
    client.SetBearerAuthentication(CreateJwt(m_sSecret));
     auto resp = client.Run();

    if(auto js = ConvertToJson(resp.data.Get()); js)
    {
        if(resp.nHttpCode == 200 && (*js)[jsonConsts::settings].isObject() && 
                                    (*js)[jsonConsts::settings][jsonConsts::path].isObject() && 
                                    (*js)[jsonConsts::settings][jsonConsts::path][jsonConsts::current].isString())
        {
            m_sPathWav = (*js)[jsonConsts::settings][jsonConsts::path][jsonConsts::current].asString();
            pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Wav files are located at " << m_sPathWav;
        }
        else
        {
            pmlLog(pml::LOG_CRITICAL, LOG_PREFIX) << "Could not locate audio path. Code=" << resp.nHttpCode;
        }
    }
    else
    {
        pmlLog(pml::LOG_CRITICAL, LOG_PREFIX) << "Could not locate audio path. Invalid JSON. Code=" << resp.nHttpCode;
    }
}

void LaunchManager::EnumLoggers()
{
    pmlLog(pml::LOG_INFO, LOG_PREFIX) << "EnumLoggers...";

    std::scoped_lock<std::mutex> lg(m_mutex);
    m_mLaunchers.clear();

    //Ask VAM for a list of loggers
    auto client = pml::restgoose::HttpClient(pml::restgoose::GET, endpoint(m_sHttpProtocol+"://"+m_sVamUrl+"/x-api/plugins/destinations"));
    client.SetBearerAuthentication(CreateJwt(m_sSecret));

    auto resp = client.Run();

    if(auto js = ConvertToJson(resp.data.Get()); js)
    {
        if(resp.nHttpCode == 200 && js->isArray())
        {
            for(const auto& jsPlugin : *js)
            {
                GetDestinationDetails("destinations/"+jsPlugin.asString());
            }
        }
        else
        {
            pmlLog(pml::LOG_CRITICAL, LOG_PREFIX) << "Cannot get list of recorders: " << resp.nHttpCode;
        }
    }
    else
    {
        pmlLog(pml::LOG_CRITICAL, LOG_PREFIX) << "Cannot get list of recorders - invalid JSON: " << resp.nHttpCode << " " << resp.data;
    }
}

void LaunchManager::GetDestinationDetails(const std::string& sPlugin)
{
    auto client = pml::restgoose::HttpClient(pml::restgoose::GET, endpoint(m_sHttpProtocol+"://"+m_sVamUrl+"/x-api/plugins/"+sPlugin));
    client.SetBearerAuthentication(CreateJwt(m_sSecret));

    auto resp = client.Run();
    if(auto js = ConvertToJson(resp.data.Get()); js)
    {
        if(resp.nHttpCode == 200)
        {
            if((*js)[jsonConsts::plugin] == "Recorder" && (*js)[jsonConsts::name].isString() && (*js)[jsonConsts::user_data].isObject() && (*js)[jsonConsts::patch_version].isUInt64())
            {
                if(auto itRecorder = m_mRecorders.find((*js)[jsonConsts::name].asString()); itRecorder == m_mRecorders.end())
                {   //no recorder
                    m_mRecorders.try_emplace((*js)[jsonConsts::name].asString(), (*js)[jsonConsts::patch_version].asUInt64());
                    CheckLoggerConfig((*js)[jsonConsts::name].asString(), (*js)[jsonConsts::user_data]);
                }
                else
                {
                    pmlLog(pml::LOG_WARN, LOG_PREFIX) << "Attempting to add " << (*js)[jsonConsts::name].asString() << " but it already exists";
                }
            }
        }
        else
        {
            pmlLog(pml::LOG_ERROR, LOG_PREFIX) << "Cannot get details of " << sPlugin << " error=" << resp.nHttpCode << " " << resp.data;
        }
    }
    else
    {
        pmlLog(pml::LOG_ERROR, LOG_PREFIX) << "Cannot get details of " << sPlugin << ". Invalid JSON error=" << resp.nHttpCode << " " << resp.data;
    }
}


std::filesystem::path LaunchManager::MakeSocketFullPath(const std::string& sEncoder) const
{
    std::filesystem::path path(m_sPathSockets);
    path /= sEncoder;
    return path;
}

void LaunchManager::LaunchAll()
{
    std::scoped_lock<std::mutex> lg(m_mutex);

    pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Start all encoders";

    for(const auto& [sName, pLauncher] : m_mLaunchers)
    {
        LaunchEncoder(pLauncher);
    }
    PipeThread();

}

void LaunchManager::LaunchEncoder(std::shared_ptr<Launcher> pLauncher) const
{
    if(pLauncher->IsRunning() == false)
    {
        pLauncher->LaunchEncoder();
    }
}

void LaunchManager::PipeThread()
{
    pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Pipe Thread check if running";
    if(m_pThread)
    {
        m_context.stop();
        m_pThread->join();

        m_pThread = nullptr;
    }

    pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Pipe Thread now start";
    m_pThread = std::make_unique<std::thread>([this]()
    {
        pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Pipe Thread started";

        auto work = asio::require(m_context.get_executor(), asio::execution::outstanding_work_t::tracked);


        if(m_context.stopped())
        {
            m_context.restart();
        }
        m_context.run();

        pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Pipe Thread stopped";
    });
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


pml::restgoose::response LaunchManager::RestartEncoder(const std::string& sName)
{
    if(auto itLauncher = m_mLaunchers.find(sName); itLauncher != m_mLaunchers.end())
    {
        itLauncher->second->RestartEncoder();
        return pml::restgoose::response(200);
    }
    return pml::restgoose::response(404, sName+" not found");
}

void LaunchManager::ExitCallback(const std::string& sEncoder, int nExitCode, bool bRemove)
{
    pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Encoder " << sEncoder << " has exited with exit code " << nExitCode;
    if(bRemove)
    {
        m_mLaunchers.erase(sEncoder);
    }
    m_exitCallback(sEncoder, nExitCode, bRemove);
}

void LaunchManager::CheckLoggerConfig(const std::string& sRecorder, const Json::Value& jsUserData)
{
    for(const auto& [sType, sApp] : m_mEncoderApps)
    {
        if(jsUserData[sType].isConvertibleTo(Json::uintValue))
        {
            if(jsUserData[sType].asUInt() != 0)
            {
                CreateLauncher(sRecorder, sType, sApp);
            }
            else
            {
                StopEncoder(sRecorder, sType);
            }
        }
    }
}

void LaunchManager::CreateLauncher(const std::string& sRecorder, const std::string& sType, const std::string& sApp)
{
    pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Logger " << sRecorder << " requires a " << sType << " encoder";
    
    auto sEncoder = sRecorder+"_"+sType;
    if(m_mLaunchers.find(sEncoder) == m_mLaunchers.end())
    {
        auto itLauncher = m_mLaunchers.try_emplace(sEncoder, std::make_shared<Launcher>(m_context, sApp, sRecorder)).first;
        itLauncher->second->SetPaths(m_sPathWav, m_sPathEncoded, MakeSocketFullPath(sEncoder));
        itLauncher->second->SetCallbacks(m_statusCallback, std::bind(&LaunchManager::ExitCallback, this, _1,_2,_3));

        if(m_encoderCallback)
        {
            m_encoderCallback(sRecorder, sType, true);
        }

        if(m_pThread)   //if thread is created then we can launch straigh away
        {
            LaunchEncoder(itLauncher->second);
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN, LOG_PREFIX) << "Logger " << sRecorder << " already has an encoder of that type";
    }
}

Json::Value LaunchManager::GetEncoderVersions() const
{
    Json::Value jsEncoders;
    for(const auto& [sType, sApp] : m_mEncoderApps)
    {
        auto js = ConvertToJson(Exec(sApp+" -v"));
        if(js)
        {
            jsEncoders[sType] = *js;
        }
    }
    return jsEncoders;
}


bool LaunchManager::WebsocketConnection(const endpoint& anEnpoint, bool bConnected, int nError)
{
    if(bConnected)
    {
        pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Websocket connected to vam";
    }
    else
    {
        pmlLog(pml::LOG_ERROR, LOG_PREFIX) << "Websocket not connected to vam : error=" << nError;
    }
    return true;
}

bool LaunchManager::WebsocketMessage(const endpoint& anEndpoint, const std::string& sMessage)
{
    if(auto js = ConvertToJson(sMessage); js)
    {
        if((*js)[jsonConsts::update].isArray())
        {
            for(const auto& jsPlugin : (*js)[jsonConsts::update])
            {
                CheckUpdate(jsPlugin);
            }
        }
    }
    return true;
}

void LaunchManager::CheckUpdate(const Json::Value& jsPlugin)
{
    if(jsPlugin[jsonConsts::path].isString() && jsPlugin[jsonConsts::type].isUInt() && jsPlugin[jsonConsts::patch_version].isUInt64())
    {
        auto vSplit = SplitString(jsPlugin[jsonConsts::path].asString(), '/', 2);
        if(vSplit.size() == 2 && vSplit[0] == "destinations")
        {
            switch(static_cast<enumUpdate>(jsPlugin[jsonConsts::type].asUInt()))
            {
                case enumUpdate::ADD:
                    GetDestinationDetails(jsPlugin[jsonConsts::path].asString());
                    break;
                case enumUpdate::REMOVE:
                    CheckForRecorderRemoval(vSplit[1]);
                    break;
                case enumUpdate::PATCH:
                    CheckForRecorderUpdate(vSplit[1], jsPlugin[jsonConsts::patch_version].asUInt64());
                    break;
            }
        }
    }
}

void LaunchManager::CheckForRecorderRemoval(const std::string& sRecorder)
{
    for(const auto& [sEncoder, pLauncher] : m_mLaunchers)
    {
        auto vSplit = SplitString(sEncoder, '_');
        if(vSplit.empty() == false && vSplit[0] == sRecorder)
        {
            pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Recorder " << sRecorder << " has been removed. Stop encoder " << sEncoder;
            pLauncher->RemoveEncoder();
        }
    }
}

void LaunchManager::StopEncoder(const std::string& sRecorder, const std::string& sType)
{
    if(auto itLauncher = m_mLaunchers.find(sRecorder+"_"+sType); itLauncher != m_mLaunchers.end())
    {
        pmlLog(pml::LOG_INFO, LOG_PREFIX) << "Recorder " << sRecorder << " no longer needs encoder type " << sType;
        itLauncher->second->RemoveEncoder();
    }
}

void LaunchManager::CheckForRecorderUpdate(const std::string& sRecorder, uint64_t nPatchVersion)
{
    if(auto itRecorder = m_mRecorders.find(sRecorder); itRecorder != m_mRecorders.end() && itRecorder->second < nPatchVersion)
    {
        auto client = pml::restgoose::HttpClient(pml::restgoose::GET, endpoint(m_sHttpProtocol+"://"+m_sVamUrl+"/x-api/plugins/destinations/"+sRecorder));
        client.SetBearerAuthentication(CreateJwt(m_sSecret));

        auto resp = client.Run();
        if(auto js = ConvertToJson(resp.data.Get()); js)
        {
            if(resp.nHttpCode == 200)
            {
                if((*js)[jsonConsts::name].isString() && (*js)[jsonConsts::user_data].isObject() && (*js)[jsonConsts::patch_version].isUInt64())
                {
                   //recorder but got newer details
                    itRecorder->second = (*js)[jsonConsts::patch_version].asUInt64();
                    CheckLoggerConfig((*js)[jsonConsts::name].asString(), (*js)[jsonConsts::user_data]);
                }
            }
        }
    }
}