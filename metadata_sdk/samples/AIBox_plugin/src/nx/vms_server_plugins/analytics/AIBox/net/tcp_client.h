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
public:
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    void connect(const std::string& host, unsigned short port, const std::string& subscribePath, 
                 const std::string& basicAuth, DataReceivedCallback callback);

    void unsubscribe();

    void disconnect();

    bool isConnected() const;
    
    void setRetryIntervalSec(int seconds);

public:
    TcpClient();
    ~TcpClient();

private:
    void onConnect(const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints);
    
    void sendSubscribeRequest(const asio::ip::tcp::resolver::results_type& endpoints);

    void onSubscribeSent(const asio::error_code& ec, size_t bytesTransferred);

    void handleHeader(size_t bytesTransferred);

    void processBody(const std::string& body);

    void handleBody(const std::string& body, bool isSubscriptionResponse);

    void readNextHeader();
    
    void sendUnsubscribeRequest();
    
    void doDisconnect();

private:
    size_t parseContentLength(const std::string& header);

    void handleResponse(const std::string& statusCode, int& retryCount, int maxRetries, 
                        const std::function<void()>& requestFunction, const std::function<void()>& onSuccess);

    void handleSubscribeFailed();

    void handleUnsubscribeFailed();

    void retryRequest(int& retryCount, int maxRetries, const std::function<void()>& requestFunction);

private:
    asio::io_context                                            m_ioContext;
    asio::executor_work_guard<asio::io_context::executor_type>  m_work_guard;
    asio::ip::tcp::resolver                                     m_resolver;
    asio::steady_timer                                          m_retryTimer;
    std::unique_ptr<asio::ip::tcp::socket>                      m_socket;
    asio::streambuf                                             m_responseBuffer;

private:
    std::thread             m_ioThread;
    mutable std::mutex      m_mutex;
    bool                    m_connected = false;
    std::string             m_host;
    unsigned short          m_port = 0;
    std::string             m_subscribePath;
    std::string             m_basicAuth;
    DataReceivedCallback    m_dataReceivedCallback;
    std::string             m_firstLine;
    std::string             m_statusCode;
    int                     m_subscribeRetryCount = 0; 
    int                     m_unsubscribeRetryCount = 0;
    std::string             m_subscriptionServerAddress;
    int                     m_retryIntervalMillisec = kReconnectDelayMillisec;

    enum class State
    {
        Disconnected,
        Connecting,
        Subscribing,
        Subscribed,
        Unsubscribing,
        Failed
    };
    State     m_state = State::Disconnected;

private:
    static const std::string    kBasicAuthPrefix;       // "Basic "
    static const std::string    kXmlVersion;            // "1.7"
    static const int            kReconnectDelayMillisec;     // reconnect delay (second)
    static const std::string    kUserAgent;             // "AIBox_plugin"
    static const int            kReTryTimes;            // 3
};

#endif // TCP_CLIENT_H