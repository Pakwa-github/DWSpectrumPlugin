#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <asio/executor_work_guard.hpp>
#include <mutex>
#include <string>
#include <functional>
#include <memory>

using DataReceivedCallback = std::function<void(const std::string&)>;

class TcpClient
{
public:
    
    static TcpClient& getInstance()
    {
        static TcpClient instance;
        return instance;
    }

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    void connect(const std::string& host, unsigned short port,
                 const std::string& subscribePath, DataReceivedCallback callback);
    
    void sendData(const std::string& data);
    
    void disconnect();

    bool isConnected() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_connected;
    }

private:
    TcpClient();
    ~TcpClient();

    void onConnect(const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints);
    
    void onSubscribeSent(const asio::error_code& ec, size_t bytesTransferred);
    
    void onReceiveHeader(const asio::error_code& ec, size_t bytesTransferred);

    void onReceiveBody(const asio::error_code& ec, size_t bytesTransferred);

private:
    asio::io_context m_ioContext;
    std::unique_ptr<asio::ip::tcp::socket> m_socket;
    asio::ip::tcp::resolver m_resolver;
    std::thread m_ioThread;
    mutable std::mutex m_mutex;
    bool m_connected = false;
    std::string m_host;
    unsigned short m_port = 0;
    std::string m_subscribePath;
    std::string m_basicAuth;
    DataReceivedCallback m_dataReceivedCallback;
    asio::streambuf m_responseBuffer;

    asio::executor_work_guard<asio::io_context::executor_type> m_work_guard;
};

#endif // TCP_CLIENT_H