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

        void Init(const iniManager& iniConfig, std::function<void(const std::string&, const Json::Value&)> statusCallback, std::function<void(const std::string&, int)> exitCallback);

        void LaunchAll();
        void StopAll();

        pml::restgoose::response AddLogger(const pml::restgoose::response& theData);
        pml::restgoose::response RemoveLogger(const std::string& sName);
        pml::restgoose::response RestartLogger(const std::string& sName);

        pml::restgoose::response GetLoggerConfig(const std::string& sName);
        pml::restgoose::response UpdateLoggerConfig(const std::string& sName, const Json::Value& jsData);

        const std::map<std::string, std::shared_ptr<Launcher>>& GetLaunchers() const {return m_mLaunchers;}

        Json::Value GetStatusSummary() const;


    private:
        void PipeThread();

        void EnumLoggers();
        std::filesystem::path MakeConfigFullPath(const std::string& sLogger);
        std::filesystem::path MakeSocketFullPath(const std::string& sLogger);

        void LaunchLogger(std::shared_ptr<Launcher> pLauncher);

        void CreateLoggerConfig(const Json::Value& jsData);


        std::filesystem::path m_pathLaunchers;
        std::filesystem::path m_pathSdp;
        std::filesystem::path m_pathSockets;
        std::filesystem::path m_pathAudio;

        std::map<std::string, std::shared_ptr<Launcher>> m_mLaunchers;


        int m_nLogConsoleLevel = 2;
        int m_nLogFileLevel = 2;
        int m_nSnmpPort = 161;
        int m_nSnmpTrapPort = 162;
        std::string m_sSnmpBaseOid = ".1.3.6.1.4.1.2";
        std::string m_sSnmpCommunity = "public";
        std::string m_sLogPath = "/var/log/loggers/";
        int m_nHeartbeatGap = 10000;
        std::string m_sInterface = "eth0";
        int m_nAoipBuffer = 4096;

        std::mutex m_mutex;
        std::unique_ptr<std::thread> m_pThread = nullptr;
        std::atomic<bool> m_bRun = true;

        std::function<void(const std::string&, const Json::Value&)> m_statusCallback = nullptr;
        std::function<void(const std::string&, int)> m_exitCallback = nullptr;

        asio::io_context m_context;
};
