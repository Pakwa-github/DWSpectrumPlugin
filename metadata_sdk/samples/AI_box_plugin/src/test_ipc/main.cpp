// Small test executable to check Subscriber IPC connection
#include "nx/vms_server_plugins/analytics/AIbox/net/subscriber.h"
#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    std::cout << "Starting IPC subscription test...\n";

    try {
        asio::io_context temp_ioc;
        asio::ip::tcp::resolver resolver(temp_ioc);
        auto endpoints = resolver.resolve("10.1.60.137", "8080");
        asio::ip::tcp::socket socket(temp_ioc);
        asio::connect(socket, endpoints);
        std::cout << "IPC 10.1.60.137:8080 reachable\n";
        socket.close();
    } catch (const std::exception& e) {
        std::cerr << "IPC 10.1.60.137:8080 unreachable: " << e.what() << "\n";
        return 1;
    }


    // Start subscription to a host:port/path. Adjust address if needed.
    Subscriber::startIpcSubscription("10.1.60.137", 8080, "/SetSubscribe");

    // Wait a bit to let connection establish and data (if any) arrive.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Stop subscription and exit.
    Subscriber::stopIpcSubscription();

    std::cout << "IPC subscription test finished.\n";
    return 0;
}
