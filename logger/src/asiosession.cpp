#include "asiosession.h"
#include "log.h"

AsioSession::AsioSession(asio::local::stream_protocol::socket sock) : m_socket(std::move(sock))
{
}

void AsioSession::start() const
{
    //do_read();
}

void AsioSession::do_read()
{
    auto self(shared_from_this());
    m_socket.async_read_some(asio::buffer(data_), [self](std::error_code, std::size_t)
    {
        //Not doing any reading
    });
}

void AsioSession::write(const std::string& sMessage)
{
    try
    {
        asio::write(m_socket, asio::buffer(sMessage.data(), sMessage.length()));
    }
    catch(asio::system_error& e)
    {
        pmlLog(pml::LOG_ERROR) << "Socket write error: " << e.what();
    }
}


AsioServer::AsioServer(const std::filesystem::path& file) :
    m_acceptor(m_context, asio::local::stream_protocol::endpoint(file.string())),
    m_timer(m_context)
{
    do_accept();
}

void AsioServer::do_accept()
{
    try
    {

        m_acceptor.async_accept(
            [this](std::error_code ec, asio::local::stream_protocol::socket socket)
            {
              if (!ec)
              {
                m_pSession = std::make_shared<AsioSession>(std::move(socket));
              }

              do_accept();
            });
    }
    catch(asio::system_error &e)
    {
        pmlLog(pml::LOG_ERROR) << "Socket accept error: " << e.what();
    }
}

void AsioServer::Run()
{
    m_pThread = std::make_unique<std::thread>([this]()
    {
        m_context.run();
    });

}

AsioServer::~AsioServer()
{
    m_context.stop();
    if(m_pThread)
    {
        m_pThread->join();
    }
}

void AsioServer::write(const std::string& sMessage)
{
    if(m_pSession)
    {
        m_pSession->write(sMessage);
    }
}

void AsioServer::StartTimer(const std::chrono::milliseconds& duration, const std::function<void()>& callback)
{
    m_timer.cancel();
    m_timer.expires_from_now(duration);
    m_timer.async_wait([callback](const asio::error_code& ec){
                       if(!ec)
                        {
                            callback();
                        }
                       });
}

void AsioServer::StopTimer()
{
    m_timer.cancel();
}
