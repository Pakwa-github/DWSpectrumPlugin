#define NX_DEBUG_ENABLE_OUTPUT true
#include <nx/kit/debug.h>

#include "tcp_client.h"
#include <iostream>
#include <sstream>
#include <chrono>

// Thread-safe printing helpers
void TcpClient::printLocked(const std::string& msg) const
{
    std::lock_guard<std::mutex> lk(m_printMutex);
    NX_PRINT << msg;
}

void TcpClient::outputLocked(const std::string& msg) const
{
    std::lock_guard<std::mutex> lk(m_printMutex);
    NX_OUTPUT << msg;
}

std::mutex TcpClient::s_instanceMutex;

const std::string TcpClient::kBasicAuthPrefix = "Basic ";
const std::string TcpClient::kXmlVersion = "1.7";
const int TcpClient::kReconnectDelaySec = 5;
const std::string TcpClient::kUserAgent = "AI_box_plugin";

TcpClient::TcpClient():
    m_resolver(m_ioContext),
    m_work_guard(asio::make_work_guard(m_ioContext)),
    m_connected(false),
    m_port(0),
    m_socket(nullptr)
{
    m_ioThread = std::thread([this]() {
        try
        {
            m_ioContext.run();
        }
        catch(const std::exception& e)
        {
            std::ostringstream oss;
            oss << "TCP IO Thread Exception: " << e.what();
            outputLocked(oss.str());
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
        printLocked("TcpClient already connected.");
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

                printLocked("Async connect starting.");
                asio::async_connect(*m_socket, results,
                    [this, results](const asio::error_code& ec, const asio::ip::tcp::endpoint& /*endpoint*/)
                    {
                        // Forward the connect result and the resolved results to onConnect
                        onConnect(ec, results);
                    });
            }
            else
            {
                std::ostringstream oss;
                oss << "Resolve error: " << ec.message();
                outputLocked(oss.str());
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

    printLocked("TcpClient connected.");

        // Send subscription request
        std::ostringstream request;
        request << "POST " << m_subscribePath << " HTTP/1.1\r\n";
        request << "Authorization: " << kBasicAuthPrefix << m_basicAuth << "\r\n";
        request << "User-Agent: " << kUserAgent << "\r\n";
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
        std::ostringstream oss;
        oss << "Connect error: " << ec.message();
        outputLocked(oss.str());
        m_connected = false;
    }
}

void TcpClient::onSubscribeSent(const asio::error_code& ec, size_t bytesTransferred)
{
    if (!ec)
    {
        {
            std::ostringstream oss;
            oss << "Subscribe request sent (" << bytesTransferred << " bytes).";
            printLocked(oss.str());
        }
        // Start reading response header
        asio::async_read_until(*m_socket, m_responseBuffer, "\r\n\r\n",
                               std::bind(&TcpClient::onReceiveResponse, this, std::placeholders::_1, std::placeholders::_2));
    }
    else
    {
        std::ostringstream oss;
        oss << "Subscribe send error: " << ec.message();
        outputLocked(oss.str());
        disconnect();
    }
}

void TcpClient::onReceiveResponse(const asio::error_code& ec, size_t bytesTransferred)
{
    if (!ec)
    {
        // We just read the initial response header (async_read_until in onSubscribeSent).
        // Parse status line and headers, then read the body according to Content-Length.
        std::istream responseStream(&m_responseBuffer);

        // Read status line
        std::string statusLine;
        std::getline(responseStream, statusLine);

        // Read remaining header lines
        std::string line;
        std::string header;
        while (std::getline(responseStream, line) && line != "\r")
            header += line + "\n";

        {
            std::ostringstream oss;
            oss << "Received response header (" << bytesTransferred << " bytes):\n" << statusLine << "\n" << header;
            printLocked(oss.str());
        }

        // Parse Content-Length
        size_t contentLength = 0;
        std::istringstream hs(header);
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
                    val = val.substr(start, end - start + 1);
                try { contentLength = static_cast<size_t>(std::stoul(val)); } catch (...) { contentLength = 0; }
                break;
            }
        }

        // body bytes already present in streambuf (if any)
        size_t bodyAlready = m_responseBuffer.size();
        if (contentLength > 0)
        {
            if (bodyAlready >= contentLength)
            {
                // full body already received
                std::ostringstream bodyoss;
                bodyoss << &m_responseBuffer; // extracts and consumes body bytes
                std::string body = bodyoss.str();
                handleBody(body, true);

                // start the continuous POST-reading loop
                startPostReadLoop();
            }
            else
            {
                // need to read remaining bytes of body
                size_t toRead = contentLength - bodyAlready;
                asio::async_read(*m_socket, m_responseBuffer, asio::transfer_exactly(toRead),
                                 [this](const asio::error_code& ec, size_t /*bytesTransferred*/)
                                 {
                                     if (!ec)
                                     {
                                         std::ostringstream bodyoss;
                                         bodyoss << &m_responseBuffer;
                                         std::string body = bodyoss.str();
                                         this->handleBody(body, true);
                                         this->startPostReadLoop();
                                     }
                                     else
                                     {
                                         std::ostringstream oss;
                                         oss << "Receive initial body error: " << ec.message();
                                         this->outputLocked(oss.str());
                                         this->disconnect();
                                     }
                                 });
            }
        }
        else
        {
            // No Content-Length: extract whatever is present and continue with loop
            std::ostringstream bodyoss;
            bodyoss << &m_responseBuffer;
            std::string body = bodyoss.str();
            if (!body.empty())
                handleBody(body, true);
            startPostReadLoop();
        }
    }
    else
    {
        std::ostringstream oss;
        oss << "Receive response error: " << ec.message();
        outputLocked(oss.str());
        disconnect();
    }
}

void TcpClient::startPostReadLoop()
{
    // Read next POST header
    asio::async_read_until(*m_socket, m_responseBuffer, "\r\n\r\n",
                           [this](const asio::error_code& ec, size_t bytesTransferred)
                           {
                               this->onPostHeader(ec, bytesTransferred);
                           });
}

void TcpClient::onPostHeader(const asio::error_code& ec, size_t bytesTransferred)
{
    if (!ec)
    {
        std::istream responseStream(&m_responseBuffer);
        std::string requestLine;
        std::getline(responseStream, requestLine);

        std::string line;
        std::string header;
        while (std::getline(responseStream, line) && line != "\r")
            header += line + "\n";

        {
            std::ostringstream oss;
            oss << "Received POST header (" << bytesTransferred << " bytes):\n" << requestLine << "\n" << header;
            printLocked(oss.str());
        }

        // Parse Content-Length
        size_t contentLength = 0;
        std::istringstream hs(header);
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
                    val = val.substr(start, end - start + 1);
                try { contentLength = static_cast<size_t>(std::stoul(val)); } catch (...) { contentLength = 0; }
                break;
            }
        }

        size_t bodyAlready = m_responseBuffer.size();
        if (contentLength > 0)
        {
            if (bodyAlready >= contentLength)
            {
                std::ostringstream bodyoss;
                bodyoss << &m_responseBuffer;
                std::string body = bodyoss.str();
                handleBody(body, false);
                // Continue loop
                startPostReadLoop();
            }
            else
            {
                size_t toRead = contentLength - bodyAlready;
                asio::async_read(*m_socket, m_responseBuffer, asio::transfer_exactly(toRead),
                                 [this](const asio::error_code& ec, size_t /*bytesTransferred*/)
                                 {
                                     if (!ec)
                                     {
                                         std::ostringstream bodyoss;
                                         bodyoss << &m_responseBuffer;
                                         std::string body = bodyoss.str();
                                         this->handleBody(body, false);
                                         this->startPostReadLoop();
                                     }
                                     else
                                     {
                                         std::ostringstream oss;
                                         oss << "Receive POST body error: " << ec.message();
                                         this->outputLocked(oss.str());
                                         this->disconnect();
                                     }
                                 });
            }
        }
        else
        {
            // No Content-Length, try to extract whatever is present and continue
            std::ostringstream bodyoss;
            bodyoss << &m_responseBuffer;
            std::string body = bodyoss.str();
            if (!body.empty())
                handleBody(body, false);
            startPostReadLoop();
        }
    }
    else
    {
        std::ostringstream oss;
        oss << "Post header read error: " << ec.message();
        outputLocked(oss.str());
        disconnect();
    }
}

void TcpClient::handleBody(const std::string& body, bool isInitialResponse)
{
    if (isInitialResponse)
    {
        // extract <serverAddress> content as CDATA or text
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
                if (start != std::string::npos) start += 1;
                end = body.find("</serverAddress>", start);
            }

            if (start != std::string::npos && end != std::string::npos && end > start)
            {
                std::string addr = body.substr(start, end - start);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_subscriptionServerAddress = addr;
                }
                std::ostringstream oss;
                oss << "Extracted subscription server address: " << addr;
                printLocked(oss.str());
            }
        }

        // We don't forward the initial response body to the data callback; it's for setup.
    }
    else
    {
        std::ostringstream oss;
        oss << "Received body (" << body.size() << " bytes)." << "\n" << body;
        printLocked(oss.str());
        // Forward XML body to the callback for parsing
        if (m_dataReceivedCallback)
            m_dataReceivedCallback(body);
    }
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
        printLocked("TcpClient disconnected.");
    }
}

void TcpClient::unsubscribe()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected || !m_socket || m_subscriptionServerAddress.empty())
    {
        outputLocked("Unsubscribe failed: not connected or no server address");
        return;
    }

    // Send unsubscribe request
    std::ostringstream request;
    request << "POST " << "/SetUnSubscribe" << " HTTP/1.1\r\n";
    request << "Authorization: " << kBasicAuthPrefix << m_basicAuth << "\r\n";
    request << "User-Agent: " << kUserAgent << "\r\n";
    request << "Host: " << m_host << "\r\n";

    std::string body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<config version=\"1.7\" xmlns=\"http://www.ipc.com/ver10\">\n"
                       "    <serverAddress>" + m_subscriptionServerAddress + "</serverAddress>\n"
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
                printLocked("Unsubscribe request sent.");
                asio::async_read_until(
                    *m_socket, 
                    m_responseBuffer, 
                    "\r\n\r\n",
                    [this](const asio::error_code& ec, size_t /*bytesTransferred*/)
                    {
                        if (!ec)
                        {
                            std::string response((std::istreambuf_iterator<char>(&m_responseBuffer)),
                                               std::istreambuf_iterator<char>());
                            std::ostringstream oss;
                            oss << "Unsubscribe response:\n" << response;
                            printLocked(oss.str());
                        }
                        else
                        {
                            std::ostringstream oss;
                            oss << "Unsubscribe response error: " << ec.message();
                            outputLocked(oss.str());
                        }
                        disconnect();
                    });
            }
            else
            {
                std::ostringstream oss;
                oss << "Unsubscribe send error: " << ec.message();
                outputLocked(oss.str());
            }
            // Disconnect after unsubscribe
            disconnect();
        });
}