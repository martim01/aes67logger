#pragma once
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <filesystem>
#include <chrono>
#include "json/json.h"

class Launcher
{
    public:
        Launcher(const std::filesystem::path& pathConfig, std::function<void(const std::string&, const Json::Value&)> statusCallback, std::function<void(const std::string&, int)> m_exitCallback);
        ~Launcher();

        void SetLoggerApp(const std::string& sLoggerApp) { m_sLoggerApp = sLoggerApp;   }

        int LaunchLogger();
        bool StopLogger();

        bool IsRunning() const;

        int GetReadPipe() const;
        void PipeRead();
        bool ClosePipeIfInactive();
        void Read();

        const Json::Value& GetJsonStatus() const { return m_jsStatus;}
        const Json::Value& GetStatusSummary() const;

        enum exitCode{PIPE_OPEN_ERROR=-4, FORK_ERROR=-3, PIPE_CLOSED=-1};
    private:

        void CloseLogger();
        void CreateSummary();


        pid_t m_pid;
        int m_nPipe[2];
        enum {READ=0, WRITE};

        int m_nExitCode;

        std::filesystem::path m_pathConfig;
        std::string m_sConfigArg;
        std::string m_sLoggerApp;

        std::string m_sOut;
        std::string m_sError;

        std::chrono::time_point<std::chrono::system_clock> m_tpLastMessage;

        std::function<void(const std::string&, const Json::Value&)> m_statusCallback;
        std::function<void(const std::string&, int)> m_exitCallback;

        Json::Value m_jsStatus;
        Json::Value m_jsStatusSummary;
};

