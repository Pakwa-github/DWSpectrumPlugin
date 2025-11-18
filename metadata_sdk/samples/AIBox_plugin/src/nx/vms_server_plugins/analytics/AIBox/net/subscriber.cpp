
#include "subscriber.h"

#include <iostream>
#include <thread>
#include <chrono>

#include <nx/kit/debug.h>

Subscriber::Subscriber() {}

Subscriber::~Subscriber() {}

void Subscriber::registerPEAResultCallback(PEAResultCallback callback)
{
    m_PEAResultCallback = callback;
}

void Subscriber::startIpcSubscription(const std::string& host, unsigned short port, const std::string& subscribePath, const std::string& basicAuth)
{
    NX_PRINT << "Starting IPC subscription to " << host << ":" << port << subscribePath;
    if (m_client.isConnected())
    {
        NX_PRINT << "Already connected. No action taken.";
        return;
    }
    m_client.connect(host, port, subscribePath, basicAuth,
        [this](const std::string& data) {
            PEAResult result = parsePEATrajectoryData(data);
            if (m_PEAResultCallback && !result.trajects.empty())
            {
                m_PEAResultCallback(result);
            }
        });
}

void Subscriber::stopIpcSubscription()
{
    NX_PRINT << "Stopping IPC subscription.";
    m_client.unsubscribe();
    const int kMaxWaitMs = 300;
    const int kStepMs = 20;
    int waited = 0;
    while (waited < kMaxWaitMs && m_client.isConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kStepMs));
        waited += kStepMs;
    }
    NX_PRINT << waited << " ms waited for unsubscribe.";
    if (m_client.isConnected())
    {
        m_client.disconnect();
    }
}
