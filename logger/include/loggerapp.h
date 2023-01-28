#pragma once
#include "portaudio.h"
#include <string>
#include "soundfile.h"
#include <chrono>
#include "inimanager.h"
#include "session.h"
#include "aoipclient.h"
#include <filesystem>

class AsioServer;

namespace Json
{
    class Value;
}

namespace pml
{
    namespace aoip
    {
        class timedbuffer;
        class AoipClient;
        class AoIPSource;
    }
}

class LoggerApp
{
    public:

        LoggerApp();
        ~LoggerApp();


        bool Init(const std::filesystem::path& config);
        int Run();

        void Exit();
    private:


        bool LoadConfig(const std::filesystem::path& config);

        void CreateLogging();

        void StartRecording();
        bool OpenSoundFile();

        void Log();


        void WriteToSoundFile(std::shared_ptr<pml::aoip::AoIPSource> pSource, std::shared_ptr<pml::aoip::timedbuffer> pBuffer);
        void QoSCallback(std::shared_ptr<pml::aoip::AoIPSource> pSource, std::shared_ptr<pml::aoip::qosData> pData);
        void SessionCallback(std::shared_ptr<pml::aoip::AoIPSource> pSource,  const pml::aoip::session& theSession);
        void StreamCallback(std::shared_ptr<pml::aoip::AoIPSource> pSource, bool bStreaming);
        void LoopCallback(std::chrono::microseconds duration);

        void OutputQoSJson(std::shared_ptr<pml::aoip::qosData> pData);
        void OutputSessionJson();
        void OutputStreamJson(bool bStreaming);
        void OutputHeartbeatJson();
        void OutputFileJson();

        Json::Value GetSubsessionJson(const pml::aoip::subsession& theSubSession);

        void StreamFail();


        std::string CreateFileName(const std::chrono::system_clock& tp);

        std::string m_sName;
        std::filesystem::path m_pathWav;
        std::filesystem::path m_pathSockets;

        std::string m_sEndpoint;
        std::string m_sInterface;

        std::chrono::milliseconds m_heartbeatGap = std::chrono::milliseconds(10000);
        std::chrono::microseconds m_timeSinceLastHeartbeat = std::chrono::microseconds(0);
        std::chrono::time_point<std::chrono::system_clock> m_tpStart;

        unsigned long m_nBufferSize = 4096;

        SoundFile m_sf;

        bool m_bHeartbeat = true;

        iniManager m_config;

        pml::aoip::session m_session;
        pml::aoip::subsession m_subsession;

        std::unique_ptr<pml::aoip::AoipClient> m_pClient = nullptr;

        int m_nLogOutputConsole = -1;
        int m_nLogOutputFile = -1;

        bool m_bReceivingAudio = false;

        std::shared_ptr<AsioServer> m_pServer = nullptr;

        unsigned int m_nFrameSize = 0;
        double m_dFrameDuration = 0.0;

};


