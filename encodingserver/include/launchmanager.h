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

#include "websocketclient.h"

class Launcher;
class iniManager;
class iniSection;
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

        Json::Value GetEncoderVersions() const;

    private:
        void PipeThread();

        void GetWavPath();
        void EnumLoggers();
        void GetDestinationDetails(const std::string& sPlugin);

        std::filesystem::path MakeSocketFullPath(const std::string& sEncoder) const;

        void ExitCallback(const std::string& sEncoder, int nExitCode, bool bRemove);

        void LaunchEncoder(std::shared_ptr<Launcher> pLauncher) const;

        void CheckLoggerConfig(const std::string& sRecorder, const Json::Value& jsUserData);

        void CreateLauncher(const std::string& sRecorder, const std::string& sType, const std::string& sApp);
        void StopEncoder(const std::string& sRecorder, const std::string& sType);

        bool WebsocketConnection(const endpoint& anEnpoint, bool bConnected, int nError);
        bool WebsocketMessage(const endpoint& anEndpoint, const std::string& sMessage);

        void CheckUpdate(const Json::Value& jsUpdate);
        void CheckForRecorderRemoval(const std::string& sRecorder);
        void CheckForRecorderUpdate(const std::string& sRecorder, uint64_t nPatchVersion);
        
        
        std::string m_sPathSockets;
        std::string m_sPathWav;
        std::string m_sPathEncoded;

        std::map<std::string, std::string> m_mEncoderApps;
        std::map<std::string, std::shared_ptr<Launcher>> m_mLaunchers;
        std::map<std::string, uint64_t> m_mRecorders;
       
        std::mutex m_mutex;
        std::unique_ptr<std::thread> m_pThread = nullptr;

        std::function<void(const std::string&, const std::string&, bool bAdded)> m_encoderCallback = nullptr;
        std::function<void(const std::string&, const Json::Value&)> m_statusCallback = nullptr;
        std::function<void(const std::string&, int, bool)> m_exitCallback = nullptr;

        asio::io_context m_context;

        std::string m_sVamUrl;
        std::string m_sSecret;
        std::string m_sHttpProtocol{"http"};
        std::string m_sWsProtocol{"ws"};

        pml::restgoose::WebSocketClient m_client;

        enum class enumUpdate {REMOVE, ADD, PATCH};

        static const std::string LOG_PREFIX;
};
