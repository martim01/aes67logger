#include "launchmanager.h"
#include "launcher.h"
#include "log.h"
#include "inimanager.h"
#include "jsonconsts.h"
#include "aes67utils.h"
#include "inisection.h"
#include "httpclient.h"

using namespace std::placeholders;

LaunchManager::LaunchManager()=default;

LaunchManager::~LaunchManager()
{
    if(m_pThread)
    {
        m_context.stop();
        m_pThread->join();
    }
    //Encoders will shutdown in the launcher destructor
}

void LaunchManager::Init(const iniManager& iniConfig, const std::function<void(const std::string&, const std::string&, bool bAdded)>& encoderCallback, const std::function<void(const std::string&, const Json::Value&)>& statusCallback, 
const std::function<void(const std::string&, int, bool)>& exitCallback)
{
    m_pathSockets.assign(iniConfig.Get(jsonConsts::path, jsonConsts::sockets, "/var/local/loggers/sockets"));
    m_pathAudio.assign(iniConfig.Get(jsonConsts::path, jsonConsts::audio, "/var/local/loggers/audio"));

    m_nLoggerConsoleLevel = iniConfig.Get(jsonConsts::logger, jsonConsts::console, -1l);
    m_nLoggerFileLevel = iniConfig.Get(jsonConsts::logger, jsonConsts::file, 2l);

    m_statusCallback = statusCallback;
    m_exitCallback = exitCallback;
    m_encoderCallback = encoderCallback;

   
    if(auto pSection = iniConfig.GetSection(jsonConsts::encoders); pSection)
    {
        for(const auto& [sType, sPath] : pSection->GetData())
        {
            m_mEncoderApps.try_emplace(sType, sPath);
        }
    }


    EnumLoggers();
    WatchLoggerPath();
    LaunchAll();

}

void LaunchManager::EnumLoggers()
{
    pmlLog(pml::LOG_INFO, "aes67") << "EnumLoggers...";

    std::scoped_lock<std::mutex> lg(m_mutex);

    //Ask VAM for a list of loggers



    try
    {
        m_mLaunchers.clear();

        for(const auto& entry : std::filesystem::directory_iterator(m_pathLaunchers))
        {
            if(entry.path().extension() == ".ini")
            {
                pmlLog(pml::LOG_INFO, "aes67") << "Logger: '" << entry.path().stem() << "' found";

                CheckLoggerConfig(entry.path());
            }   
        }
    }
    catch(std::filesystem::filesystem_error& e)
    {
        pmlLog(pml::LOG_CRITICAL, "aes67") << "Could not enum Loggers..." << e.what();
    }
}

void LaunchManager::WatchLoggerPath()
{
    pmlLog(pml::LOG_INFO, "aes67") << "Watch " << m_pathLaunchers << " for loggers being created or deleted";

    auto nWatch = m_observer.AddWatch(m_pathLaunchers, pml::filewatch::Observer::CREATED | pml::filewatch::Observer::DELETED, false);
    if(nWatch != -1)
    {
        m_observer.AddWatchHandler(nWatch, std::bind(&LaunchManager::OnLoggerCreated, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::CREATED);
        m_observer.AddWatchHandler(nWatch, std::bind(&LaunchManager::OnLoggerDeleted, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::DELETED);
    }
    else
    {
        pmlLog(pml::LOG_ERROR, "aes67") << "Could not created watch";
    }
    m_observer.Run();
}

std::filesystem::path LaunchManager::MakeConfigFullPath(const std::string& sLogger) const
{
    auto path = m_pathLaunchers;
    path /= std::string(sLogger+".ini");
    return path;
}

std::filesystem::path LaunchManager::MakeSocketFullPath(const std::string& sEncoder) const
{
    auto path = m_pathSockets;
    path /= sEncoder;
    return path;
}

void LaunchManager::LaunchAll()
{
    std::scoped_lock<std::mutex> lg(m_mutex);

    pmlLog(pml::LOG_INFO, "aes67") << "Start all encoders";

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
    pmlLog(pml::LOG_INFO, "aes67") << "Pipe Thread check if running";
    if(m_pThread)
    {
        m_context.stop();
        m_pThread->join();

        m_pThread = nullptr;
    }

    pmlLog(pml::LOG_INFO, "aes67") << "Pipe Thread now start";
    m_pThread = std::make_unique<std::thread>([this]()
    {
        pmlLog(pml::LOG_INFO, "aes67") << "Pipe Thread started";

        auto work = asio::require(m_context.get_executor(), asio::execution::outstanding_work_t::tracked);


        if(m_context.stopped())
        {
            m_context.restart();
        }
        m_context.run();

        pmlLog(pml::LOG_INFO, "aes67") << "Pipe Thread stopped";
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
    pmlLog(pml::LOG_INFO, "aes67") << "Encoder " << sEncoder << " has exited with exit code " << nExitCode;
    if(bRemove)
    {
        m_mLaunchers.erase(sEncoder);
    }
    m_exitCallback(sEncoder, nExitCode, bRemove);
}

void LaunchManager::OnLoggerCreated(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory)
{
    if(path.extension() == ".ini")
    {
        CheckLoggerConfig(path);       
    }
}

void LaunchManager::OnLoggerDeleted(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory)
{
    //@todo onloggerdeleted
}

void LaunchManager::CheckLoggerConfig(const std::filesystem::path& pathConfig)
{
    iniManager config;
    if(config.Read(pathConfig))
    {
        if(auto pSection = config.GetSection(jsonConsts::keep); pSection)
        {
            LaunchEncoders(pathConfig, pSection);
        }
    }
}

void LaunchManager::LaunchEncoders(const std::filesystem::path& pathConfig, std::shared_ptr<iniSection> pSection)
{
    for(const auto& [sType, sValue] : pSection->GetData())
    {
        if(sType != jsonConsts::wav)
        {
            try
            {
                if(auto nHours = std::stoul(sValue); nHours > 0)
                {
                    LaunchEncoder(pathConfig, sType);
                }
            }
            catch(std::exception& e)
            {
                pmlLog(pml::LOG_WARN, "aes67") << "Keep: " << sType << " is invalid " << sValue;
            }
        }
    }
}
void LaunchManager::LaunchEncoder(const std::filesystem::path& pathConfig, const std::string& sType)
{
    pmlLog(pml::LOG_INFO, "aes67") << "Logger " << pathConfig.stem().string() << " requires a " << sType << " encoder";
    auto itApp = m_mEncoderApps.find(sType);
    if(itApp != m_mEncoderApps.end())
    {
        auto sEncoder = pathConfig.stem().string()+"_"+sType;

        m_mLaunchers.try_emplace(sEncoder, std::make_shared<Launcher>(m_context, itApp->second, pathConfig,
                                                                                     MakeSocketFullPath(sEncoder),
                                                                                     m_statusCallback, std::bind(&LaunchManager::ExitCallback, this, _1,_2,_3)));

        if(m_encoderCallback)
        {
            m_encoderCallback(pathConfig.stem(), sType, true);
        }
    }
    else
    {
        pmlLog(pml::LOG_ERROR, "aes67") << "No encoder app associated with " << sType;
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