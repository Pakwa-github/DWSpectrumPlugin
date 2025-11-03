#define NX_DEBUG_ENABLE_OUTPUT true
#include <nx/kit/debug.h>

#include "subscriber.h"
#include <iostream>

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
            NX_PRINT << "Received data: " << data;
        });
}

void Subscriber::stopIpcSubscription()
{
    TcpClient::getInstance().disconnect();
}
