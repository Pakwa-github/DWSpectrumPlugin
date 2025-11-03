#define NX_DEBUG_ENABLE_OUTPUT true
#include <nx/kit/debug.h>

#include "tcp_client.h"
#include <iostream>
#include <sstream>
#include <chrono>

TcpClient::TcpClient():
    m_resolver(m_ioContext),
    m_work_guard(asio::make_work_guard(m_ioContext))
{
    m_ioThread = std::thread([this]() {
        try
        {
            m_ioContext.run();
        }
        catch(const std::exception& e)
        {
            NX_OUTPUT << "TCP IO Thread Exception: " << e.what();
        }
    });
}

TcpClient::~TcpClient()
{
    disconnect();
    m_work_guard.reset();
    // m_ioContext.stop();
    if (m_ioThread.joinable())
        m_ioThread.join();
}

void TcpClient::connect(const std::string& host, unsigned short port,
                        const std::string& subscribePath, DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connected)
    {
        NX_PRINT << "TcpClient already connected.";
        return;
    }

    m_host = host;
    m_port = port;
    m_subscribePath = subscribePath;
    m_dataReceivedCallback = callback;

    m_resolver.async_resolve(host, std::to_string(port),
        [this](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results)
        {
            if (!ec)
            {
                m_socket = std::make_unique<asio::ip::tcp::socket>(m_ioContext);

                NX_PRINT << "Async connect starting.";
                asio::async_connect(*m_socket, results,
                    [this, results](const asio::error_code& ec, const asio::ip::tcp::endpoint& /*endpoint*/)
                    {
                        // Forward the connect result and the resolved results to onConnect
                        onConnect(ec, results);
                    });
            }
            else
            {
                NX_OUTPUT << "Resolve error: " << ec.message();
            }
        });

    return;
}

void TcpClient::onConnect(const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (!ec)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_connected = true;
        }

        m_basicAuth = "aTNhZG1pbjpBZG1pbiEyMw=="; // "i3admin:Admin!23" base64 encoded

        NX_PRINT << "TcpClient connected.";

        // Send subscription request
        std::ostringstream request;
        request << "POST " << m_subscribePath << " HTTP/1.1\r\n";
        request << "Authorization: Basic " << m_basicAuth << "\r\n";
        request << "User-Agent: AI_box_plugin\r\n";
        if (!endpoints.empty())
        {
            request << "Host: " << endpoints.begin()->host_name() << "\r\n";
        }
        else
        // todo
        {
            request << "Host: " << "?" << "\r\n";
        }
        // only support PEA and ALARM_FEATURE for now
        std::string body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<config version=\"1.7\" xmlns=\"http://www.ipc.com/ver10\">\n"
                           "    <subscribeFlag>BASE_SUBSCRIBE</subscribeFlag>\n"
                           "    <subscribeList>\n"
                           "        <item>\n"
                           "            <smartType>PEA</smartType>\n"
                           "            <subscribeRelation>ALARM_FEATURE</subscribeRelation>\n"
                           "        </item>\n"
                           "    </subscribeList>\n"
                           "    <generalSubscribe>\n"
                           "        <subscribeRelation>trajectory</subscribeRelation>\n"
                           "    </generalSubscribe>\n"
                           "</config>\n";
        request << "Content-Length: " << body.size() << "\r\n";
        request << "Connection: keep-alive\r\n";
        request << "Keep-Alive: 300\r\n";
        request << "\r\n";
        request << body;
        
        asio::async_write(*m_socket, asio::buffer(request.str()), 
                          std::bind(&TcpClient::onSubscribeSent, this, std::placeholders::_1, std::placeholders::_2));
    }
    else
    {
        NX_OUTPUT << "Connect error: " << ec.message();
        m_connected = false;
    }
}

void TcpClient::onSubscribeSent(const asio::error_code& ec, size_t bytesTransferred)
{
    if (!ec)
    {
        NX_PRINT << "Subscribe request sent (" << bytesTransferred << " bytes).";
        // Start reading response header
        asio::async_read_until(*m_socket, m_responseBuffer, "\r\n\r\n",
                               std::bind(&TcpClient::onReceiveHeader, this, std::placeholders::_1, std::placeholders::_2));
    }
    else
    {
        NX_OUTPUT << "Subscribe send error: " << ec.message();
        disconnect();
    }
}

void TcpClient::onReceiveHeader(const asio::error_code& ec, size_t bytesTransferred)
{
    if (!ec)
    {
        std::string header((std::istreambuf_iterator<char>(&m_responseBuffer)),
                           std::istreambuf_iterator<char>());
        NX_PRINT << "Received header (" << bytesTransferred << " bytes):\n" << header;
        asio::async_read(*m_socket, m_responseBuffer, asio::transfer_at_least(1),
                         std::bind(&TcpClient::onReceiveBody, this, std::placeholders::_1, std::placeholders::_2));
    }
    else
    {
        NX_OUTPUT << "Receive header error: " << ec.message();
        disconnect();
    }
}

void TcpClient::onReceiveBody(const asio::error_code& ec, size_t bytesTransferred)
{
    if (!ec)
    {
        std::string data((std::istreambuf_iterator<char>(&m_responseBuffer)),
                         std::istreambuf_iterator<char>());
        NX_PRINT << "Received body (" << bytesTransferred << " bytes):\n" << data;

        if (m_dataReceivedCallback)
        {
            m_dataReceivedCallback(data);
        }

        // Continue reading more data
        asio::async_read(*m_socket, m_responseBuffer, asio::transfer_at_least(1),
                         std::bind(&TcpClient::onReceiveBody, this, std::placeholders::_1, std::placeholders::_2));
    }
    else
    {
        NX_OUTPUT << "Receive body error: " << ec.message();
        disconnect();

        // 5s later, try to reconnect
        NX_PRINT << "Reconnecting in 5 seconds...";
        std::this_thread::sleep_for(std::chrono::seconds(5));
        this->connect(m_host, m_port, m_subscribePath, m_dataReceivedCallback);
        return;
    }
}

void TcpClient::sendData(const std::string& data)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected || !m_socket)
    {
        NX_OUTPUT << "TcpClient not connected. Cannot send data.";
        return;
    }

    asio::async_write(*m_socket, asio::buffer(data),
                      [](const asio::error_code& ec, size_t bytesTransferred)
                      {
                          if (ec)
                          {
                              NX_OUTPUT << "Send data error: " << ec.message();
                          }
                          else
                          {
                              NX_PRINT << "Sent data (" << bytesTransferred << " bytes).";
                          }
                      });
}

void TcpClient::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connected && m_socket)
    {
        asio::error_code ec;
        m_socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        m_socket->close(ec);
        m_connected = false;
        NX_PRINT << "TcpClient disconnected.";
    }
}