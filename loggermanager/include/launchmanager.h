#pragma once
#include <string>
#include <map>
#include <memory>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include "json/json.h"
#include "response.h"
#include "asio.hpp"


class Launcher;
class iniManager;

class LaunchManager
{
    public:
        LaunchManager();
        ~LaunchManager();

        void Init(const iniManager& iniConfig, const std::function<void(const std::string&, const Json::Value&)>& statusCallback, 
        const std::function<void (const std::string& , int, bool)>& exitCallback);

        void LaunchAll();

        pml::restgoose::response AddLogger(const pml::restgoose::response& theData);
        pml::restgoose::response RemoveLogger(const std::string& sName);
        pml::restgoose::response RestartLogger(const std::string& sName);

        pml::restgoose::response GetLoggerConfig(const std::string& sName) const;
        pml::restgoose::response UpdateLoggerConfig(const std::string& sName, const Json::Value& jsData);

        const std::map<std::string, std::shared_ptr<Launcher>>& GetLaunchers() const {return m_mLaunchers;}

        Json::Value GetStatusSummary() const;



    private:
        void PipeThread();

        void EnumLoggers();
        std::filesystem::path MakeConfigFullPath(const std::string& sLogger) const;
        std::filesystem::path MakeSocketFullPath(const std::string& sLogger) const;

        void ExitCallback(const std::string& sLogger, int nExitCode, bool bRemove);

        void LaunchLogger(std::shared_ptr<Launcher> pLauncher) const;

        void CreateLoggerConfig(const Json::Value& jsData) const;


        
        std::filesystem::path m_pathLaunchers;
        std::filesystem::path m_pathSdp;
        std::filesystem::path m_pathSockets;
        std::filesystem::path m_pathAudio;

        std::map<std::string, std::shared_ptr<Launcher>> m_mLaunchers;


        ssize_t m_nLogConsoleLevel = -1;
        ssize_t m_nLogFileLevel = 2;
        std::string m_sLogPath = "/var/log/loggers/";
        
        long m_nHeartbeatGap = 10000;
        long m_nAoipBuffer = 4096;
        bool m_bUseTransmissionTime = false;
        long m_nLoggerConsoleLevel = -1;
        long m_nLoggerFileLevel = 2;

        std::string m_sLoggerInterface = "eth0";

        std::mutex m_mutex;
        std::unique_ptr<std::thread> m_pThread = nullptr;

        std::function<void(const std::string&, const Json::Value&)> m_statusCallback = nullptr;
        std::function<void(const std::string&, int, bool)> m_exitCallback = nullptr;


        asio::io_context m_context;
};
