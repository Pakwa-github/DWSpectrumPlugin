#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include "tcp_client.h"
#include "net_utils.h"
#include <string>

class Subscriber {
public:
    static void startIpcSubscription(const std::string& host = "10.1.60.137",
                                     unsigned short port = 8080,
                                     const std::string& subscribePath = "/SetSubscribe");

    static void stopIpcSubscription();

    static bool isSubscribed()
    {
        return TcpClient::getInstance().isConnected();
    }

public:
    using PEAResultCallback = std::function<void(const std::vector<PEAResult>&)>;

    static void registerPEAResultCallback(PEAResultCallback callback);

private:
    static PEAResultCallback s_PEAResultCallback;
};

#endif // SUBSCRIBER_H
