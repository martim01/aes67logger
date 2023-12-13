#pragma once
//#include "portaudio.h"
#include <string>
#include "soundfile.h"
#include <chrono>
#include "inimanager.h"
#include "aoipscheduler.h"
#include <filesystem>
#include "json/json.h"
#include <mutex>

class AsioServer;

namespace Json
{
    class Value;
}

namespace pml
{
    namespace aoip
    {
        class AudioFrame;
        class InputSession;
        struct rtpStats;
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

        bool StartRecording();
        bool OpenSoundFile();


        bool StatsCallback(const std::string& sGroup, const pml::aoip::rtpStats& theStats);

        void WriteToSoundFile();
        void LoopCallback(std::chrono::microseconds duration);

        void OutputStatsJson(const std::string& sGroup, const pml::aoip::rtpStats& theStatsa);
        void OutputHeartbeatJson();
        void OutputFileJson();

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

        pml::aoip::Scheduler m_scheduler;
        std::shared_ptr<pml::aoip::InputSession> m_pSession = nullptr;
        
        int m_nLogOutputConsole = -1;
        int m_nLogOutputFile = -1;

        bool m_bReceivingAudio = false;
        bool m_bUseTransmissionTime = false;
        unsigned long m_nFileLength = 1;

        std::shared_ptr<AsioServer> m_pServer = nullptr;
	std::vector<float> m_vBuffer;

        unsigned int m_nFrameSize = 0;
        double m_dFrameDuration = 0.0;

        std::string m_sSdp;
        Json::Value m_jsStatus;

        std::chrono::microseconds m_timeSinceLastAudio = std::chrono::microseconds(0);
};


