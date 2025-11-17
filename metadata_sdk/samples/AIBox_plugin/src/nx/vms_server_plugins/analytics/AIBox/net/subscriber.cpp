
#include "subscriber.h"

#include <iostream>
#include <thread>
#include <chrono>

#include <nx/kit/debug.h>

Subscriber::PEAResultCallback Subscriber::s_PEAResultCallback = nullptr;

void Subscriber::registerPEAResultCallback(PEAResultCallback callback)
{
    s_PEAResultCallback = callback;
}

void Subscriber::startIpcSubscription(const std::string& host, unsigned short port, const std::string& subscribePath, const std::string& basicAuth)
{
    NX_PRINT << "Starting IPC subscription to " << host << ":" << port << subscribePath;
    TcpClient& client = TcpClient::getInstance();
    if (client.isConnected())
    {
        NX_PRINT << "Already subscribed to IPC.";
        return;
    }
    client.connect(host, port, subscribePath, basicAuth,
        [](const std::string& data) {
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
    const int kMaxWaitMs = 300;
    const int kStepMs = 20;
    int waited = 0;
    while (waited < kMaxWaitMs && client.isConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kStepMs));
        waited += kStepMs;
    }
    NX_PRINT << waited << " ms waited for unsubscribe.";
    if (client.isConnected())
    {
        client.disconnect();
    }
}
