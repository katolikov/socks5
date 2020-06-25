#include "spawn.hpp"
#include "socks5.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <type_traits>

template<typename... Ts>
std::string format(Ts &&... args) {
    std::ostringstream oss;
    (oss <<...<< std::forward<Ts>(args));
    return oss.str();
}

template<typename T>
std::optional<T> from_chars(std::string_view sv) noexcept {
    T out;
    auto end = sv.data() + sv.size();
    auto res = std::from_chars(sv.data(), end, out);
    if (res.ec == std::errc{} && res.ptr == end)
        return out;
    return {};
}

class logger {
public:
    template<typename T>
    logger &operator<<(T &&x) {
        std::cerr << std::forward<T>(x);
        return *this;
    }

    ~logger() {
        std::cerr << std::endl;
    }

private:
    static std::mutex m_;

    std::lock_guard<std::mutex> l_{m_};
};

std::mutex logger::m_;

using bigint = boost::multiprecision::cpp_int;

template<typename F>
auto at_scope_exit(F &&f) {
    using f_t = std::remove_cvref_t<F>;
    static_assert(std::is_nothrow_destructible_v<f_t> &&
                  std::is_nothrow_invocable_v<f_t>);
    struct ase_t {
        F f;

        ase_t(F &&f)
                : f(std::forward<F>(f)) {}

        ase_t(const ase_t &) = default;

        ase_t(ase_t &&) = delete;

        ase_t operator=(const ase_t &) = delete;

        ase_t operator=(ase_t &&) = delete;

        ~ase_t() {
            std::forward<F>(f)();
        }
    };
    return ase_t{std::forward<F>(f)};
}

int main(int argc, char *argv[]) {

    try {
        if (argc < 2)
            logger{} << format("Usage: ", argv[0], " <listen-port>");
        auto port = from_chars<std::uint16_t>(argv[1]);
        if (!port || !*port)
            logger{} << "Port must be in [1;65535]";
        auto num = from_chars<int>(argv[2]);
        if (!num)
            logger{} << "Number of client must be large then 0";
        auto num_thread = from_chars<std::size_t>(argv[3]);
        if (!num_thread)
            logger{} << "Number of thread must be large then 0";
        auto mode = from_chars<int>(argv[4]);
        if (mode > 1 || mode < 0)
            logger{} << "Mode must be in [0;1]\n"
                        "0 : small size request\n"
                        "1 : large size request\n";

        boost::asio::io_context ctx;
        boost::asio::signal_set stop_signals{ctx, SIGINT, SIGTERM};

        stop_signals.async_wait([&](boost::system::error_code ec, int /*signal*/) {
            if (ec)
                return;
            logger{} << "Terminating in response to signal.";
            ctx.stop();
        });

        int count_request = 0;
        int count_reply = 0;

        std::string request;

        if (!mode)
            request = 'h';
        else
            request.resize(1048576, 'h');

        boost::asio::ip::tcp::resolver resolver(ctx);

        auto http_endpoint = *resolver.resolve(
                boost::asio::ip::tcp::v4(), "www.example.com", "http");

        std::string ip = "127.0.0.1";
        int port_ip = 8888;

        for (auto i = 0; i < num; ++i) {

            boost::asio::ip::tcp::socket socket_new{make_strand(ctx)};

            bccoro::spawn(bind_executor(ctx,
                                        [socket = boost::beast::tcp_stream{
                                                std::move(socket_new)}, port = *port, request,
                                                count_request, count_reply, ip, port_ip, http_endpoint]
                                                (bccoro::yield_context yc) mutable {

                                            boost::asio::ip::tcp::endpoint ep{boost::asio::ip::tcp::v4(), port};
                                            boost::asio::ip::tcp::endpoint way(
                                                    boost::asio::ip::address::from_string(ip), port_ip);
                                            constexpr static std::chrono::seconds timeout{20};
                                            boost::system::error_code ec;

                                            for (;;) {

                                                socket.async_connect(ep, yc[ec]);

                                                if (ec) {
                                                    if (ec != boost::asio::error::operation_aborted)
                                                        logger{} << "Failed to connect: " << ec.message();
                                                    return;
                                                }
                                                socks5::client::request_first socks_request_first;
                                                socket.expires_after(timeout);
                                                socket.async_write_some(socks_request_first.buffers(), yc[ec]);

                                                if (ec) {
                                                    if (ec != boost::asio::error::operation_aborted)
                                                        logger{} << "Failed to write first request: "
                                                                 << ec.message();
                                                } else
                                                    count_request++;

                                                socket.expires_after(timeout);
                                                socks5::client::reply_first socks_reply_first;
                                                auto n = socket.async_read_some(socks_reply_first.buffers(), yc[ec]);

                                                if (ec) {
                                                    if (ec != boost::asio::error::operation_aborted &&
                                                        (ec != boost::asio::error::eof || n))
                                                        logger{} << "First reply error: " << ec.message();
                                                } else
                                                    count_reply++;

                                                socket.expires_after(timeout);
                                                socks5::client::request_second socks_request_second(
                                                        socks5::client::request_second::connect, way);
                                                socket.async_write_some(socks_request_second.buffers(), yc[ec]);

                                                if (ec) {
                                                    if (ec != boost::asio::error::operation_aborted)
                                                        logger{} << "Failed to write second request: "
                                                                 << ec.message();
                                                } else
                                                    count_request++;

                                                socket.expires_after(timeout);
                                                socks5::client::reply_second socks_reply_second;
                                                n = socket.async_read_some(socks_reply_second.buffers(), yc[ec]);
                                                count_reply++;

                                                if (ec) {
                                                    if (ec != boost::asio::error::operation_aborted &&
                                                        (ec != boost::asio::error::eof || n))
                                                        logger{} << "Second reply error: " << ec.message();
                                                } else
                                                    count_reply++;

                                                std::string request_new =
                                                        "GET / HTTP/1.0\r\n"
                                                        "Host: www.example.com\r\n"
                                                        "Accept: */*\r\n"
                                                        "Connection: close\r\n\r\n";

                                                // Send the HTTP request.
                                                socket.expires_after(timeout);
                                                socket.async_write_some(boost::asio::buffer(request), yc[ec]);

                                                if (ec) {
                                                    logger{} << "Write close request error: " << ec.message();
                                                } else
                                                    count_request++;

                                                // Read until EOF, writing data to output as we go.
                                                std::array<char, 1048576> response{};

                                                /*while (std::size_t s = socket.async_read_some(
                                                        boost::asio::buffer(response), yc[ec]))
                                                    std::cout.write(response.data(), s);
                                                */

                                                socket.expires_after(timeout);

                                                n = socket.async_read_some(boost::asio::buffer(response), yc[ec]);

                                                if (ec) {
                                                    if (ec != boost::asio::error::operation_aborted &&
                                                        (ec != boost::asio::error::eof || n))
                                                        logger{} << "Read close reply error: " << ec.message();
                                                } else
                                                    count_reply++;
                                            }
                                        }));
        }

        std::vector<std::thread> workers;
        std::size_t extra_workers = std::thread::hardware_concurrency();
        if (num_thread > extra_workers) {
            logger{} << "Count of choosen thread are not avalaible now.\n"
                     << extra_workers << " - thread avalaible.\n";
        } else
            extra_workers = *num_thread;

        workers.reserve(extra_workers);
        auto ase = at_scope_exit([&]() noexcept {
            for (auto &t:workers)
                t.join();
        });
        for (std::size_t i = 0; i < extra_workers; ++i)
            workers.emplace_back([&] {
                ctx.run();
            });
        ctx.run();
    }
    catch (...) {
        logger{} << boost::current_exception_diagnostic_information();
        return 1;
    }
}
