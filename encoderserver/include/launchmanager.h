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
#include "observer.h"

class Launcher;
class iniManager;

class LaunchManager
{
    public:
        LaunchManager();
        ~LaunchManager();

        void Init(const iniManager& iniConfig, 
                    const std::function<void(const std::string&, const std::string&, bool bAdded)>& encoderCallback,
                    const std::function<void(const std::string&, const Json::Value&)>& statusCallback, 
                    const std::function<void (const std::string& , int, bool)>& exitCallback);

        void LaunchAll();

        pml::restgoose::response RestartEncoder(const std::string& sName);

        
        const std::map<std::string, std::shared_ptr<Launcher>>& GetEncoders() const {return m_mLaunchers;}

        Json::Value GetStatusSummary() const;



    private:
        void PipeThread();

        void EnumLoggers();
        void WatchLoggerPath();
        void OnLoggerCreated(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);
        void OnLoggerDeleted(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);

        std::filesystem::path MakeConfigFullPath(const std::string& sEncoder) const;
        std::filesystem::path MakeSocketFullPath(const std::string& sEncoder) const;

        void ExitCallback(const std::string& sEncoder, int nExitCode, bool bRemove);

        void LaunchEncoder(std::shared_ptr<Launcher> pLauncher) const;

        void CheckLoggerConfig(const std::filesystem::path& pathConfig);

        
        
        std::filesystem::path m_pathLaunchers;
        std::filesystem::path m_pathSdp;
        std::filesystem::path m_pathSockets;
        std::filesystem::path m_pathAudio;

        std::map<std::string, std::shared_ptr<Launcher>> m_mLaunchers;


        ssize_t m_nLogConsoleLevel = -1;
        ssize_t m_nLogFileLevel = 2;
        std::string m_sLogPath = "/var/log/encoders/";
        
        long m_nHeartbeatGap = 10000;
        long m_nAoipBuffer = 4096;
        bool m_bUseTransmissionTime = false;
        long m_nLoggerConsoleLevel = -1;
        long m_nLoggerFileLevel = 2;

        std::string m_sOpusApp = "/usr/local/bin/opusencoder";
        std::string m_sFlacApp = "/usr/local/bin/flacencoder";

        std::mutex m_mutex;
        std::unique_ptr<std::thread> m_pThread = nullptr;

        std::function<void(const std::string&, const std::string&, bool bAdded)> m_encoderCallback = nullptr;
        std::function<void(const std::string&, const Json::Value&)> m_statusCallback = nullptr;
        std::function<void(const std::string&, int, bool)> m_exitCallback = nullptr;

        asio::io_context m_context;

        pml::filewatch::Observer m_observer;
};
