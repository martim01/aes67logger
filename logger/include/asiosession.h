#pragma once
#include <array>
#include <memory>
#include "asio.hpp"
#include <thread>
#include <filesystem>
#include <functional>
#include <string_view>



class AsioSession: public std::enable_shared_from_this<AsioSession>
{
public:
  explicit AsioSession(asio::local::stream_protocol::socket sock);

  void start() const;

  void write(const std::string& sMessage);

private:
  void do_read();


  // The socket used to communicate with the client.
  asio::local::stream_protocol::socket m_socket;

  // Buffer used to store data received from the client.
  std::array<char, 1024> data_;
};

class AsioServer
{
public:
    explicit AsioServer(const std::filesystem::path& file);
    void Run();
    ~AsioServer();

    void write(const std::string& sMessage);

    void StartTimer(const std::chrono::milliseconds& duration, const std::function<void()>& callback);
    void StopTimer();

private:
    void do_accept();

    asio::io_context m_context;
    asio::local::stream_protocol::acceptor m_acceptor;
    asio::steady_timer m_timer;

    std::unique_ptr<std::thread> m_pThread = nullptr;
    std::shared_ptr<AsioSession> m_pSession = nullptr;
};
