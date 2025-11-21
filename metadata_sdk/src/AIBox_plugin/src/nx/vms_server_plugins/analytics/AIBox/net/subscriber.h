#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include <string>

#include "tcp_client.h"
#include "net_utils.h"

class Subscriber {
public:
    Subscriber();
    ~Subscriber();

    void startIpcSubscription(const std::string& host = "10.1.60.137",
                              unsigned short port = 8080,
                              const std::string& subscribePath = "/SetSubscribe",
                              const std::string& basicAuth = "aTNhZG1pbjpBZG1pbiEyMw==");

    void stopIpcSubscription();

    bool isSubscribed() const { return m_client->isConnected(); }

    using PEAResultCallback = std::function<void(const PEAResult&)>;
    void registerPEAResultCallback(PEAResultCallback callback);

private:
    PEAResultCallback m_PEAResultCallback = nullptr;
    std::shared_ptr<TcpClient> m_client;
};

#endif // SUBSCRIBER_H
