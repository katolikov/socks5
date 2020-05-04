#include "client.hpp"

void client::tcpClient(int num, char *info[]) {
    try {
        if (num != 3) {
            std::cerr << "Usage: blocking_tcp_echo_client <host> <port>\n";
        }

        boost::asio::io_service io_service;

        tcp::resolver resolver(io_service);
        tcp::resolver::query query(tcp::v4(), info[1], info[2]);
        tcp::resolver::iterator iterator = resolver.resolve(query);

        tcp::socket s(io_service);
        boost::asio::connect(s, iterator);

        std::cout << "Enter message: ";
        char request[max_length];
        std::cin.getline(request, max_length);
        size_t request_length = strlen(request);
        boost::asio::write(s, boost::asio::buffer(request, request_length));

        char reply[max_length];
        size_t reply_length = boost::asio::read(s, boost::asio::buffer(reply, request_length));
        std::cout << "Reply is: ";
        std::cout.write(reply, reply_length);
        std::cout << "\n";
    }
    catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

void client::boostContextTest() {
    int a;
    boost::context::fiber source{[&a](boost::context::fiber &&sink) {
        a = 0;
        int b = 1;
        while (true) {
            sink = std::move(sink).resume();
            int next = a + b;
            a = b;
            b = next;
        }
        return std::move(sink);
    }};
    for (int j = 0; j < 10; ++j) {
        source = std::move(source).resume();
        std::cout << a << " ";
    }
}