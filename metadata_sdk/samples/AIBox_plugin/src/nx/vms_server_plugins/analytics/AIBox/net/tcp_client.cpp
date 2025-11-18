
#include "tcp_client.h"

#include <iostream>
#include <sstream>
#include <chrono>

#include <nx/kit/debug.h>

const std::string TcpClient::kBasicAuthPrefix =     "Basic ";
const std::string TcpClient::kXmlVersion =          "1.7";
const int TcpClient::kReconnectDelayMillisec =      50;
const std::string TcpClient::kUserAgent =           "AIBox_plugin";
const int TcpClient::kReTryTimes =                  3;

TcpClient::TcpClient():
    m_resolver(m_ioContext),
    m_work_guard(asio::make_work_guard(m_ioContext)),
    m_retryTimer(m_ioContext),
    m_connected(false),
    m_port(0),
    m_socket(nullptr),
    m_subscribeRetryCount(0)
{
    m_ioThread = std::thread([this]()
    {
        try
        {
            m_ioContext.run();
        }
        catch(const std::exception& e)
        {
            NX_PRINT << "TCP IO Thread Exception: " << e.what();
        }
    });
}

TcpClient::~TcpClient()
{
    disconnect();
    m_work_guard.reset();
    if (m_ioThread.joinable())
    {
        m_ioThread.join();
    } 
}

void TcpClient::connect(const std::string& host, unsigned short port, const std::string& subscribePath, 
                        const std::string& basicAuth, DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connected)
    {
        NX_PRINT << "TcpClient already connected. No action taken.";
        return;
    }

    m_host = host;
    m_port = port;
    m_subscribePath = subscribePath;
    m_basicAuth = basicAuth;
    m_dataReceivedCallback = callback;
    m_state = State::Connecting;

    m_resolver.async_resolve(
        host,
        std::to_string(port),
        [this](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results)
        {
            if (!ec)
            {
                NX_PRINT << "Async connect starting...";
                m_socket = std::make_unique<asio::ip::tcp::socket>(m_ioContext);
                asio::async_connect(
                    *m_socket,
                    results,
                    [this, results](const asio::error_code& ec, const asio::ip::tcp::endpoint& /*endpoint*/)
                    {
                        onConnect(ec, results);
                    });
            }
            else
            {
                NX_PRINT << "Resolve error: " << ec.message();
                m_connected = false;
                m_state = State::Failed;
            }
        });

    return;
}

void TcpClient::onConnect(const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (!ec)
    {
        NX_PRINT << "TcpClient connected.";
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_retryTimer.cancel();
            m_connected = true;
            m_subscribeRetryCount = 0;
            m_state = State::Subscribing;
        }
        sendSubscribeRequest(endpoints);
    }
    else
    {
        NX_PRINT << "Connect error: " << ec.message();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_connected = false;
            m_state = State::Failed;
        }
    }
}

void TcpClient::sendSubscribeRequest(const asio::ip::tcp::resolver::results_type& endpoints)
{
    std::ostringstream request;
    request << "POST " << m_subscribePath << " HTTP/1.1\r\n";
    request << "Authorization: " << kBasicAuthPrefix << m_basicAuth << "\r\n";
    request << "User-Agent: " << kUserAgent << "\r\n";
    if (!endpoints.empty())
    {
        request << "Host: " << endpoints.begin()->host_name() << "\r\n";
    }
    else if (m_socket)
    {
        auto ep = m_socket->remote_endpoint();
        request << "Host: " << ep.address().to_string() << ":" << ep.port() << "\r\n";
    }
    else
    {
        request << "Host: " << m_host << "\r\n";
    }
    // only support PEA and ALARM_FEATURE
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

void TcpClient::onSubscribeSent(const asio::error_code& ec, size_t bytesTransferred)
{
    if (!ec)
    {
        NX_PRINT << "Subscribe request sent (" << bytesTransferred << " bytes).";
        asio::async_read_until(
            *m_socket,
            m_responseBuffer,
            "\r\n\r\n",
            [this](const asio::error_code& ec, size_t bytesTransferred)
            {
                if (!ec)
                {
                    this->handleHeader(bytesTransferred);
                }
                else
                {
                    NX_PRINT << "Response read error: " << ec.message();
                    this->handleSubscribeFailed();
                }
            });
    }
    else
    {
        NX_PRINT << "Subscribe send error: " << ec.message();
        handleSubscribeFailed();
    }
}

void TcpClient::handleHeader(size_t bytesTransferred)
{
    std::istream responseStream(&m_responseBuffer);
    std::getline(responseStream, m_firstLine);

    size_t codeStart = m_firstLine.find(' ');
    if (codeStart != std::string::npos)
    {
        codeStart++;
    }
    size_t codeEnd = m_firstLine.find(' ', codeStart);
    m_statusCode = (codeStart != std::string::npos && codeEnd != std::string::npos && codeEnd > codeStart)
        ? m_firstLine.substr(codeStart, codeEnd - codeStart)
        : "";

    std::string line;
    std::string header;
    while (std::getline(responseStream, line) && line != "\r")
    {
        header += line + "\n";
    }
    {
        std::ostringstream oss;
        oss << "Received header (" << bytesTransferred << " bytes):\n" << m_firstLine << "\n" << header;
        // NX_PRINT << oss.str();
    }

    size_t contentLength = parseContentLength(header);
    size_t bodyAlready = m_responseBuffer.size();

    if (contentLength > 0)
    {
        if (bodyAlready >= contentLength)
        {
            std::ostringstream bodyoss;
            bodyoss << &m_responseBuffer;
            processBody(bodyoss.str());
        }
        else
        {
            size_t toRead = contentLength - bodyAlready;
            asio::async_read(
                *m_socket,
                m_responseBuffer,
                asio::transfer_exactly(toRead),
                [this](const asio::error_code& ec, size_t /*bytesTransferred*/)
                {
                    if (!ec)
                    {
                        std::ostringstream bodyoss;
                        bodyoss << &m_responseBuffer;
                        this->processBody(bodyoss.str());
                    }
                    else
                    {
                        NX_PRINT << "Body read error: " << ec.message();
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            m_state = State::Failed;
                        }
                        this->disconnect();
                    }
                });
        }
    }
    else  // No Content-Length
    {
        std::ostringstream bodyoss;
        bodyoss << &m_responseBuffer;
        std::string body = bodyoss.str();
        if (!body.empty())
        {
            processBody(body);
        }
        else
        {
            NX_PRINT << "No Content-Length & no body.";
            if (m_state == State::Subscribed)
            {
                readNextHeader();
            }
            else 
            {
                disconnect();
            }
        }
    }
}

void TcpClient::processBody(const std::string& body)
{
    switch (m_state)
    {
        case State::Subscribed:
        {
            handleBody(body, false);
            readNextHeader();
            break;
        }
        case State::Subscribing:
        {
            handleResponse(m_statusCode, m_subscribeRetryCount, kReTryTimes,
                [this]() { sendSubscribeRequest(m_resolver.resolve(m_host, std::to_string(m_port))); },
                [this, body]() 
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_state = State::Subscribed;
                    }
                    handleBody(body, true);
                    readNextHeader();
                });
            break;
        }
        case State::Unsubscribing:
        {
            NX_PRINT << "Handling unsubscribe response...";
            if (m_firstLine.find("POST") != std::string::npos)
            {
                NX_PRINT << "Ignore this header.";
                readNextHeader();
                break;
            }
            handleResponse(m_statusCode, m_unsubscribeRetryCount, kReTryTimes,
                [this]() { sendUnsubscribeRequest(); },
                [this]()
                {
                    NX_PRINT << "Unsubscribe successful.";
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_state = State::Disconnected;
                    }
                    disconnect();
                });
            break;
        }
        default:
        {    
            NX_PRINT << "Unknown state.";
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_state = State::Failed;
            }
            disconnect();
            break;
        }
    }
}

void TcpClient::handleBody(const std::string& body, bool isSubscriptionResponse)
{
    if (!isSubscriptionResponse)
    {
        if (m_dataReceivedCallback)
        {
            m_dataReceivedCallback(body);
        }
    }
    else
    {
        NX_PRINT << "Received subscription response body (" << body.size() << " bytes)." << "\n";
        size_t pos = body.find("<serverAddress");
        if (pos != std::string::npos)
        {
            size_t start = body.find("><![CDATA[", pos);
            size_t end = std::string::npos;
            if (start != std::string::npos)
            {
                start += strlen("><![CDATA[");
                end = body.find("]]>", start);
            }
            else
            {
                // fallback: find > and </serverAddress>
                start = body.find('>', pos);
                if (start != std::string::npos) 
                {
                    start += 1;
                }
                end = body.find("</serverAddress>", start);
            }
            if (start != std::string::npos && end != std::string::npos && end > start)
            {
                std::string addr = body.substr(start, end - start);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_subscriptionServerAddress = addr;
                }
                NX_PRINT << "Extracted subscription server address: " << m_subscriptionServerAddress;
            }
        }
    }
}

void TcpClient::readNextHeader()
{
    asio::async_read_until(
        *m_socket,
        m_responseBuffer,
        "\r\n\r\n",
        [this](const asio::error_code& ec, size_t bytesTransferred)
        {
            if (!ec)
                this->handleHeader(bytesTransferred);
            else
            {
                NX_PRINT << "Header read error: " << ec.message();
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_state = State::Failed;
                }
                disconnect();
            }
        });
}

void TcpClient::unsubscribe()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected || !m_socket || m_subscriptionServerAddress.empty())
    {
        NX_PRINT << "Unsubscribe failed: not connected or no server address";
        return;
    }
    m_state = State::Unsubscribing;
    m_unsubscribeRetryCount = 0;
    sendUnsubscribeRequest();
}

void TcpClient::sendUnsubscribeRequest()
{
    std::ostringstream request;
    request << "POST " << "/SetUnSubscribe" << " HTTP/1.1\r\n";
    request << "Authorization: " << kBasicAuthPrefix << m_basicAuth << "\r\n";
    request << "User-Agent: " << kUserAgent << "\r\n";
    request << "Host: " << m_host << "\r\n";
    std::string body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<config version=\"1.7\" xmlns=\"http://www.ipc.com/ver10\">\n"
                       "    <serverAddress><![CDATA[" + m_subscriptionServerAddress + "]]></serverAddress>\n"
                       "    <unsubscribeList>\n"
                       "        <item>\n"
                       "            <smartType>PEA</smartType>\n"
                       "            <subscribeRelation>ALARM_FEATURE</subscribeRelation>\n"
                       "        </item>\n"
                       "    </unsubscribeList>\n"
                       "    <generalSubscribe>\n"
                       "        <subscribeRelation>trajectory</subscribeRelation>\n"
                       "    </generalSubscribe>\n"
                       "</config>\n";
    request << "Content-Length: " << body.size() << "\r\n";
    request << "Connection: keep-alive\r\n";
    request << "Keep-Alive: 300\r\n";
    request << "\r\n";
    request << body;

    asio::async_write(
        *m_socket,
        asio::buffer(request.str()),
        [this](const asio::error_code& ec, size_t /*bytesTransferred*/)
        {
            if (!ec)
            {
                NX_PRINT << "Unsubscribe request sent...";
            }
            else
            {
                NX_PRINT << "Unsubscribe send error: " << ec.message();
                handleUnsubscribeFailed();
            }
        });
}

void TcpClient::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connected && m_socket)
    {
        m_retryTimer.cancel();
        asio::error_code ec;
        m_socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        m_socket->close(ec);
        m_connected = false;
        m_state = State::Disconnected;
        NX_PRINT << "TcpClient disconnected!";
    }
}

bool TcpClient::isConnected() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected && 
           (m_state != State::Disconnected && m_state != State::Failed);
}

void TcpClient::setRetryIntervalSec(int milliseconds)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (milliseconds > 0)
        m_retryIntervalMillisec = milliseconds;
}

size_t TcpClient::parseContentLength(const std::string& header)
{
    size_t contentLength = 0;
    std::istringstream hs(header);
    std::string line;
    while (std::getline(hs, line))
    {
        const std::string key = "Content-Length:";
        auto pos = line.find(key);
        if (pos != std::string::npos)
        {
            std::string val = line.substr(pos + key.size());
            size_t start = val.find_first_not_of(" \t");
            size_t end = val.find_last_not_of(" \t\r");
            if (start != std::string::npos && end != std::string::npos && end >= start)
            {
                val = val.substr(start, end - start + 1);
            }
            try { contentLength = static_cast<size_t>(std::stoul(val)); } catch (...) { contentLength = 0; }
            break;
        }
    }
    return contentLength;
}

void TcpClient::handleResponse(const std::string& statusCode, int& retryCount, int maxRetries, 
                               const std::function<void()>& requestFunction, const std::function<void()>& onSuccess)
{
    if (statusCode != "200")
    {
        retryRequest(retryCount, maxRetries, requestFunction);
    }
    else
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            retryCount = 0;
            m_retryTimer.cancel();
        }
        onSuccess();
    }
}

void TcpClient::handleSubscribeFailed()
{
    retryRequest(m_subscribeRetryCount, kReTryTimes, [this]() { sendSubscribeRequest(m_resolver.resolve(m_host, std::to_string(m_port))); });
}

void TcpClient::handleUnsubscribeFailed()
{
    retryRequest(m_unsubscribeRetryCount, kReTryTimes, [this]() { sendUnsubscribeRequest(); });
}

void TcpClient::retryRequest(int& retryCount, int maxRetries, const std::function<void()>& requestFunction)
{
    int rc = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        rc = retryCount;
    }
    if (rc < maxRetries)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            rc++;
            retryCount = rc;
            NX_PRINT << "Retrying request, attempt (" << retryCount << "/" << maxRetries << ")";
        }
        m_retryTimer.expires_after(std::chrono::milliseconds(m_retryIntervalMillisec));
        m_retryTimer.async_wait(
            [this, requestFunction](const asio::error_code& ec)
            {
                if (ec == asio::error::operation_aborted)
                    return;
                if (!ec)
                {
                    try
                    {
                        requestFunction();
                    }
                    catch (const std::exception& e)
                    {
                        NX_PRINT << "Exception in retry requestFunction: " << e.what();
                    }
                }
                else
                {
                    NX_PRINT << "Retry timer error: " << ec.message();
                }
            });
    }
    else
    {
        NX_PRINT << "Max retries reached. Disconnecting...";
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = State::Failed;
        }
        disconnect();
    }
}