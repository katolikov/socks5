#include "client.hpp"
#include "socks5.hpp"

void client::tcpClient(int num, char *info[]) {
    try {
        if (num != 4) {
            std::cout << "Usage: sync_client <socks4server> <socks4port> <user>\n";
            std::cout << "Examples:\n";
            std::cout << "  sync_client 127.0.0.1 1080 chris\n";
            std::cout << "  sync_client localhost socks chris\n";
        }

        boost::asio::io_context io_context;

        // Get a list of endpoints corresponding to the SOCKS 5 server name.
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(info[1], info[2]);

        // Try each endpoint until we successfully establish a connection to the
        // SOCKS 5 server.
        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        // Get an endpoint for the Boost website. This will be passed to the SOCKS
        // 4 server.
        auto http_endpoint = *resolver.resolve(tcp::v4(), "www.boost.org", "http");

        // Send the request to the SOCKS 5 server.
        socks5::request socks_request(socks5::request::connect, http_endpoint, info[3]);
        boost::asio::write(socket, socks_request.buffers());

        // Receive a response from the SOCKS 5 server.
        socks5::reply socks_reply;
        boost::asio::read(socket, socks_reply.buffers());

        // Check whether we successfully negotiated with the SOCKS 5 server.
        if (!socks_reply.success()) {
            std::cout << "Connection failed.\n";
            std::cout << "status = 0x" << std::hex << socks_reply.status();
        }

        // Form the HTTP request. We specify the "Connection: close" header so that
        // the server will close the socket after transmitting the response. This
        // will allow us to treat all data up until the EOF as the response.
        std::string request =
                "GET / HTTP/1.0\r\n"
                "Host: www.boost.org\r\n"
                "Accept: */*\r\n"
                "Connection: close\r\n\r\n";

        // Send the HTTP request.
        boost::asio::write(socket, boost::asio::buffer(request));

        // Read until EOF, writing data to output as we go.
        std::array<char, 512> response;
        boost::system::error_code error;
        while (std::size_t s = socket.read_some(
                boost::asio::buffer(response), error))
            std::cout.write(response.data(), s);
        if (error != boost::asio::error::eof)
            throw std::system_error(error);
    }
    catch (std::exception &e) {
        std::cout << "Exception: " << e.what() << "\n";
    }
}





/*void client::boostContextTest() {
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
}*/