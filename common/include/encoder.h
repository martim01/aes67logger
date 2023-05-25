#pragma once
#include <filesystem>
#include <queue>
#include "inimanager.h"
#include "asiosession.h"
#include <thread>
#include "observer.h"

namespace Json{ class Value; }

class Encoder
{
    public:
        explicit Encoder(const std::string& sType);
        virtual ~Encoder();


        bool Init(const std::filesystem::path& config);
        int Run();
        void Exit() { m_bRun = false;}


    protected:

        bool LoadConfig(const std::filesystem::path& config);
        void CreateLogging();

        void CreateInitialFileQueue();
        void StartObserver();
        

        virtual bool InitEncoder()=0;
        virtual bool EncodeFile(const std::filesystem::path& wavFile)=0;

        void OnWavWritten(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);

        void SendError(const std::string& sMessage, const std::filesystem::path& path) const;
        void OutputEncodedStats(const std::filesystem::path& wavFile, double dDone) const;
        void OutputJson(const std::filesystem::path& wavFile, Json::Value& jsStatus) const;

        std::filesystem::path GetNextFile();

        const std::string& GetType() const { return m_sType; }
        const std::string& GetName() const { return m_sName; }
        int GetLogToConsole() const { return m_nLogToConsole; }
        int GetLogToFile() const { return m_nLogToFile; }

        std::shared_ptr<AsioServer> GetAsioServer() const { return m_pServer;}
        size_t GetBufferSize() const { return m_nBufferSize; }
        
        const std::filesystem::path& GetPathSockets() const { return m_pathSockets; }
        const std::filesystem::path& GetPathWav() const { return m_pathWav; }
        const std::filesystem::path& GetPathEncoded() const { return m_pathEncoded; }


        void ClearSamplesEncoded() { m_nSamplesEncoded = 0;}
        void SamplesEncoded(size_t nSamples) { m_nSamplesEncoded += nSamples;};
        size_t GetSamplesEncoded() const { return m_nSamplesEncoded;}

        void FileEncoded(const std::filesystem::path& pathWav);
    private:

        std::string m_sType;
        std::string m_sName;
        int m_nLogToConsole = -1;
        int m_nLogToFile = -1;

        std::shared_ptr<AsioServer> m_pServer = nullptr;

        std::queue<std::filesystem::path> m_qToEncode;
        size_t m_nBufferSize = 65536;

        iniManager m_config;
        std::filesystem::path m_pathSockets;
        std::filesystem::path m_pathWav;
        std::filesystem::path m_pathEncoded;

        std::mutex m_mutex;
        std::condition_variable m_condition;

        pml::filewatch::Observer m_observer;
        std::chrono::time_point<std::chrono::system_clock> m_tpStart;

        bool m_bRun = true;

        size_t m_nSamplesEncoded = 0;
        size_t m_nFilesEncoded = 0;
        std::filesystem::path m_lastEncoded;
};
