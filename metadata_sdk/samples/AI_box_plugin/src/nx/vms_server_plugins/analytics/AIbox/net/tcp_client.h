#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <mutex>
#include <string>
#include <functional>
#include <memory>
#include <sstream>

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <asio/executor_work_guard.hpp>


using DataReceivedCallback = std::function<void(const std::string&)>;

class TcpClient
{
private:
    static std::mutex s_instanceMutex;

public:
    
    static TcpClient& getInstance()
    {
        std::lock_guard<std::mutex> lock(s_instanceMutex);
        static TcpClient instance;
        return instance;
    }

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    void connect(const std::string& host, unsigned short port,
                 const std::string& subscribePath, DataReceivedCallback callback);
        
    void disconnect();

    bool isConnected() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_connected;
    }

    void unsubscribe();

private:
    TcpClient();
    ~TcpClient();

    void onConnect(const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints);
    
    void onSubscribeSent(const asio::error_code& ec, size_t bytesTransferred);

    void onReceiveResponse(const asio::error_code& ec, size_t bytesTransferred);

    // Start the loop that reads POST messages from the server (read header then exact body)
    void startPostReadLoop();

    // Handler for a POST/response header that has been read into m_responseBuffer
    void onPostHeader(const asio::error_code& ec, size_t bytesTransferred);

    // Process a complete body. If isInitialResponse==true, extract serverAddress, else forward to callback
    void handleBody(const std::string& body, bool isInitialResponse);

private:
    asio::io_context m_ioContext;
    std::unique_ptr<asio::ip::tcp::socket> m_socket;
    asio::ip::tcp::resolver m_resolver;
    asio::streambuf m_responseBuffer;
    asio::executor_work_guard<asio::io_context::executor_type> m_work_guard;

private:
    std::thread m_ioThread;
    mutable std::mutex m_mutex;
    bool m_connected = false;
    std::string m_host;
    unsigned short m_port = 0;
    std::string m_subscribePath;
    std::string m_basicAuth;
    DataReceivedCallback m_dataReceivedCallback;
    std::string m_subscriptionServerAddress;

private:
    static const std::string kBasicAuthPrefix;  // "Basic "
    static const std::string kXmlVersion;       // "1.7"
    static const int kReconnectDelaySec;        // reconnect delay (second)
    static const std::string kUserAgent;        // "AI_box_plugin"

private:
    mutable std::mutex m_printMutex;
    // Thread-safe printing helpers. Use these to ensure each log line is printed atomically.
    void printLocked(const std::string& msg) const;
    void outputLocked(const std::string& msg) const;
};

#endif // TCP_CLIENT_H