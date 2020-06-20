#include "client.hpp"
#include "socks5.hpp"

void client::tcpClient(int num, char *info[]) {
    try {
        if (num != 4) {
            std::cout << "Connect: " << info[1] << ' ' << info[2] << "\n";
        }

        boost::asio::io_context io_context;

        std::string ip_addr = info[1];
        std::string port_num = info[2];

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(ip_addr, port_num);

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        auto http_endpoint = *resolver.resolve(tcp::v4(), "ya.ru", "https");

        socks5::request_first socks_request_first;
        boost::asio::write(socket, socks_request_first.buffers());

        // Receive a response first from the SOCKS 5 server.
        socks5::reply_first socks_reply_first;

        boost::asio::read(socket, socks_reply_first.buffers());

        if (!socks_reply_first.success()) {
            std::cout << "Connection failed.\n";
            std::cout << socks_reply_first.status();
        }

        socks5::request_second socks_request_second(socks5::request_second::connect, http_endpoint);
        boost::asio::write(socket, socks_request_second.buffers());

        // Receive a response from the SOCKS 5 server.
        socks5::reply_second socks_reply_second;
        boost::asio::read(socket, socks_reply_second.buffers());

        // Check whether we successfully negotiated with the SOCKS 5 server.
        if (!socks_reply_second.success()) {
            std::cout << "Connection failed.\n";
            std::cout << socks_reply_second.status();
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
        std::array<char, 512> response{};
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





