#define NX_DEBUG_ENABLE_OUTPUT true
#include <nx/kit/debug.h>

#include "subscriber.h"
#include <iostream>

Subscriber::PEAResultCallback Subscriber::s_PEAResultCallback = nullptr;

void Subscriber::registerPEAResultCallback(PEAResultCallback callback)
{
    s_PEAResultCallback = callback;
}

void Subscriber::startIpcSubscription(const std::string& host, unsigned short port, const std::string& subscribePath)
{
    NX_PRINT << "Starting IPC subscription to " << host << ":" << port << subscribePath;
    TcpClient& client = TcpClient::getInstance();
    if (client.isConnected())
    {
        NX_PRINT << "Already subscribed to IPC.";
        return;
    }
    client.connect(host, port, subscribePath,
        [](const std::string& data) {
            // NX_PRINT << "Received data: " << data;

            std::vector<PEAResult> results = parsePEATrajectoryData(data);
            if (Subscriber::s_PEAResultCallback && !results.empty())
            {
                Subscriber::s_PEAResultCallback(results);
            }
        });
}

void Subscriber::stopIpcSubscription()
{
    NX_PRINT << "Stopping IPC subscription.";
    auto& client = TcpClient::getInstance();
    client.unsubscribe();
    client.disconnect();
    NX_PRINT << "IPC subscription stopped.";
}
