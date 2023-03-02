#pragma once
#include <filesystem>
#include <map>
#include "loggerobserver.h"
#include <memory>
#include "observer.h"


class PlaybackServer;

class LoggerManager
{
    public:
        LoggerManager(PlaybackServer& server);
        void EnumLoggers();
        void WatchLoggerPath();
        const std::map<std::string, std::shared_ptr<LoggerObserver>>& GetLoggers() const { return m_mLoggers;}
    
    private:
        
        std::shared_ptr<LoggerObserver> CreateLoggerObserver(const std::filesystem::path& iniFile);
        void OnLoggerCreated(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);
        void OnLoggerDeleted(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);

        PlaybackServer& m_server;
        pml::filewatch::Observer m_observer;

        std::map<std::string, std::shared_ptr<LoggerObserver>> m_mLoggers;
};