#pragma once
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <filesystem>
#include <chrono>
#include "json/json.h"
#include <array>
#include "asio.hpp"

class Launcher
{
    public:
        Launcher(asio::io_context& context, const std::filesystem::path& pathConfig, const std::filesystem::path& pathSocket, std::function<void(const std::string&, const Json::Value&)> statusCallback, std::function<void(const std::string&, int)> m_exitCallback);
        ~Launcher();


        bool CheckForOrphanedLogger();

        void SetLoggerApp(const std::string& sLoggerApp) { m_sLoggerApp = sLoggerApp;   }

        int LaunchLogger();
        bool StopLogger();

        bool IsRunning() const;

        const std::filesystem::path& GetSocketPath() const { return m_pathSocket; }
        void Read();

        const Json::Value& GetJsonStatus() const { return m_jsStatus;}
        const Json::Value& GetStatusSummary() const;


        enum exitCode{PIPE_OPEN_ERROR=-4, FORK_ERROR=-3, PIPE_CLOSED=-1};
    private:

        void CloseLogger();
        void CreateSummary();

        void Connect(const std::chrono::milliseconds& wait);
        void HandleConnect(const asio::error_code& e);
        void HandleRead(std::error_code ec, std::size_t length);
        std::vector<std::string> ExtractReadBuffer(size_t nLength);

        pid_t m_pid;
        asio::io_context& m_context;
        asio::steady_timer m_timer;

        std::filesystem::path m_pathConfig;
        std::filesystem::path m_pathSocket;
        int m_nExitCode;


        std::string m_sConfigArg;
        std::string m_sLoggerApp;

        std::string m_sOut;
        std::array<char, 4096> m_data;

        std::function<void(const std::string&, const Json::Value&)> m_statusCallback;
        std::function<void(const std::string&, int)> m_exitCallback;

        Json::Value m_jsStatus;
        Json::Value m_jsStatusSummary;

        std::mutex m_mutex;
        std::shared_ptr<asio::local::stream_protocol::socket> m_pSocket = nullptr;
};

