#include "spawn.hpp"
#include "socks5.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <vector>
#include <memory>
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
            throw std::runtime_error(format("Usage: ", argv[0], " <listen-port>"));
        auto port = from_chars<std::uint16_t>(argv[1]);
        if (!port || !*port)
            throw std::runtime_error("Port must be in [1;65535]");
        auto num = from_chars<int>(argv[2]);
        if (!num)
            throw std::runtime_error("Number of client must be large then 0");
        boost::asio::io_context ctx;
        boost::asio::signal_set stop_signals{ctx, SIGINT, SIGTERM};
        stop_signals.async_wait([&](boost::system::error_code ec, int /*signal*/) {
            if (ec)
                return;
            logger{} << "Terminating in response to signal.";
            ctx.stop();
        });

        for (auto i = 0; i < num; ++i) {

            boost::asio::ip::tcp::socket socket_new{make_strand(ctx)};

            boost::asio::ip::tcp::resolver resolver(ctx);

            auto http_endpoint = *resolver.resolve(
                    boost::asio::ip::tcp::v4(), "www.example.com", "http");

            bccoro::spawn(bind_executor(ctx,
                                        [socket = boost::beast::tcp_stream{
                                                std::move(socket_new)}, http_endpoint, port = *port]
                                                (bccoro::yield_context yc) mutable {

                                            boost::asio::ip::tcp::endpoint ep{boost::asio::ip::tcp::v4(), port};
                                            constexpr static std::chrono::seconds timeout{20};
                                            boost::system::error_code ec;

                                            for (;;) {

                                                socket.async_connect(ep, yc[ec]);

                                                if (ec == boost::asio::error::operation_aborted) {
                                                    return;
                                                }

                                                if (ec) {
                                                    logger{} << "Failed to connect: " << ec.message();
                                                }

                                                socks5::client::request_first socks_request_first;
                                                socket.expires_after(timeout);
                                                socket.async_write_some(socks_request_first.buffers(), yc[ec]);

                                                if (ec) {
                                                    logger{} << "Failed to write first request: "
                                                             << ec.message();
                                                }

                                                socket.expires_after(timeout);
                                                socks5::client::reply_first socks_reply_first;
                                                socket.async_read_some(socks_reply_first.buffers(), yc[ec]);

                                                if (ec) {
                                                    logger{} << "First reply error: " << ec.message();
                                                }

                                                socket.expires_after(timeout);
                                                socks5::client::request_second socks_request_second(
                                                        socks5::client::request_second::connect, http_endpoint);
                                                socket.async_write_some(socks_request_second.buffers(), yc[ec]);

                                                if (ec) {
                                                    logger{} << "Failed to write second request: "
                                                             << ec.message();
                                                }

                                                socket.expires_after(timeout);
                                                socks5::client::reply_second socks_reply_second;
                                                socket.async_read_some(socks_reply_second.buffers(), yc[ec]);

                                                if (ec) {
                                                    logger{} << "Second reply error: " << ec.message();
                                                }

                                                std::string request =
                                                        "GET / HTTP/1.0\r\n"
                                                        "Host: www.example.com\r\n"
                                                        "Accept: */*\r\n"
                                                        "Connection: close\r\n\r\n";

                                                // Send the HTTP request.
                                                socket.expires_after(timeout);
                                                socket.async_write_some(boost::asio::buffer(request), yc[ec]);

                                                // Read until EOF, writing data to output as we go.
                                                std::array<char, 512> response{};
                                                while (std::size_t s = socket.async_read_some(
                                                        boost::asio::buffer(response), yc[ec]))
                                                    std::cout.write(response.data(), s);
                                                if (ec) {
                                                    logger{} << "Close Error: " << ec.message();
                                                }
                                            }
                                        }));
        }
        std::vector<std::thread> workers;
        std::size_t extra_workers = std::thread::hardware_concurrency() - 1;
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
