#pragma once
#include <opusenc.h>
#include <filesystem>
#include <queue>
#include "inimanager.h"
#include "asiosession.h"
#include <thread>
#include "observer.h"


class OpusEncoder
{
    public:
        OpusEncoder() = default;
        ~OpusEncoder();


        bool Init(const std::filesystem::path& config);
        int Run();
        void Exit() { m_bRun = false;}


    private:

        bool LoadConfig(const std::filesystem::path& config);
        void CreateLogging();

        void CreateInitialFileQueue();
        void StartObserver();
        

        bool InitEncoder();
        bool EncodeFile(const std::filesystem::path& wavFile);

        void OnWavWritten(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);

        void SendError(const std::string& sMessage, const std::filesystem::path& path);

        std::filesystem::path GetNextFile();

        std::string m_sName;
        int m_nLogToConsole = -1;
        int m_nLogToFile = -1;

        OggOpusComments* m_pComments = nullptr;
        OggOpusEnc* m_pEncoder = nullptr;
        std::shared_ptr<AsioServer> m_pServer = nullptr;

        std::queue<std::filesystem::path> m_qToEncode;
        size_t m_nBufferSize = 65536;

        iniManager m_config;
        std::filesystem::path m_pathSockets;
        std::filesystem::path m_pathWav;
        std::filesystem::path m_pathOpus;

        std::mutex m_mutex;
        std::condition_variable m_condition;

        pml::filewatch::Observer m_observer;

        bool m_bRun = true;
};
