#include "loggermanager.h"
#include "inimanager.h"
#include "log.h"
#include "observer.h"
#include "playbackserver.h"
#include "jsonconsts.h"

using namespace std::placeholders;

LoggerManager::LoggerManager(PlaybackServer& server) : m_server(server)
{

}

void LoggerManager::EnumLoggers()
{
    try
    {
        for(const auto& entry : std::filesystem::directory_iterator(m_server.GetIniManager().Get(jsonConsts::path, jsonConsts::loggers,".")))
        {
            if(entry.path().extension() == ".ini")
            {
                CreateLoggerObserver(entry.path());               
            }
        }
    }
    catch(std::filesystem::filesystem_error& e)
    {
        pmlLog(pml::LOG_CRITICAL) << "Could not enum loggers..." << e.what();
    }
}

void LoggerManager::WatchLoggerPath()
{
    auto nWatch = m_observer.AddWatch(m_server.GetIniManager().Get(jsonConsts::path, jsonConsts::loggers,"."), pml::filewatch::Observer::CREATED | pml::filewatch::Observer::DELETED, false);
    if(nWatch != -1)
    {
        m_observer.AddWatchHandler(nWatch, std::bind(&LoggerManager::OnLoggerCreated, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::CREATED);
        m_observer.AddWatchHandler(nWatch, std::bind(&LoggerManager::OnLoggerDeleted, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::DELETED);
    }
    m_observer.Run();
}


std::shared_ptr<LoggerObserver> LoggerManager::CreateLoggerObserver(const std::filesystem::path& path)
{
    iniManager ini;
    if(ini.Read(path))
    {
        return m_mLoggers.try_emplace(path.stem().string(), std::make_shared<LoggerObserver>(m_server, path.stem().string(), ini, m_observer)).first->second;
    }
    else
    {
        pmlLog(pml::LOG_ERROR) << "LoggerManager - could not read " << path;
        return nullptr;
    }
}

void LoggerManager::OnLoggerCreated(int, const std::filesystem::path& path, uint32_t, bool)
{
    auto pLogger = CreateLoggerObserver(path);
    if(pLogger)
    {
        m_server.LoggerCreated(path.stem().string(), pLogger);
    }
}

void LoggerManager::OnLoggerDeleted(int, const std::filesystem::path& path, uint32_t, bool)
{
    auto itLogger = m_mLoggers.find(path.stem().string());
    if(itLogger != m_mLoggers.end())
    {
        m_server.LoggerDeleted(itLogger->first, itLogger->second);
        m_mLoggers.erase(itLogger);
    }
}


