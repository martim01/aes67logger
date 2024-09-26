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
#include <string_view>
class Launcher
{
    public:
        Launcher(asio::io_context& context, std::string_view sEncoderApp, const std::string& sRecorder);
        
        void SetPaths(const std::string& sWav, const std::string& sEncoded, const std::filesystem::path& socket);
        void SetCallbacks(const std::function<void(const std::string&, const Json::Value&)>& statusCallback, const std::function<void(const std::string&, int, bool)>& exitCallback);
        ~Launcher();


        bool CheckForOrphanedEncoder();

        void SetEncoderApp(std::string_view sEncoderApp) { m_sEncoderApp = sEncoderApp;   }

        int LaunchEncoder();
        bool RestartEncoder();
        bool RemoveEncoder();

        bool IsRunning() const;

        const std::filesystem::path& GetSocketPath() const { return m_pathSocket; }
        void Read();

        const Json::Value& GetJsonStatus() const { return m_jsStatus;}
        const Json::Value& GetStatusSummary() const;


        enum exitCode{FORK_ERROR=-1, REMOVED=1};

    private:

        void CloseAndLaunchOrRemove(int nSignal);
        int CloseEncoder(int nSignal);
        void CreateSummary();

        void DoRemoveEncoder();

        void Connect(const std::chrono::milliseconds& wait);
        void HandleConnect(const asio::error_code& e);
        void HandleRead(std::error_code ec, std::size_t length);
        std::vector<std::string> ExtractReadBuffer(size_t nLength);

        pid_t m_pid = 0;
        asio::io_context& m_context;
        asio::steady_timer m_timer;
        std::string m_sEncoderApp = "/usr/local/bin/opusencoder";
        
        std::string m_sRecorder;
        std::filesystem::path m_pathSocket;
        std::string m_sPathWav;
        std::string m_sPathEncoded;
        std::string m_sPathSocket;
        int m_nExitCode = 0;


        std::string m_sConfigArg;
        

        std::string m_sOut;
        std::array<char, 4096> m_data;

        std::function<void(const std::string&, const Json::Value&)> m_statusCallback = nullptr;
        std::function<void(const std::string&, int, bool)> m_exitCallback = nullptr;

        Json::Value m_jsStatus;
        Json::Value m_jsStatusSummary;

        std::mutex m_mutex;
        std::shared_ptr<asio::local::stream_protocol::socket> m_pSocket = nullptr;

        bool m_bMarkedForRemoval = false;
};

