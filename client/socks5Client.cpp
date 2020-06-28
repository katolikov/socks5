#include "spawn.hpp"
#include "spawn_new.hpp"
#include "socks5.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio.hpp>
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

        explicit ase_t(F &&f)
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
            logger{} << format("Usage: ", argv[0],
                               "<listen-port>", "<client-number>", "<thread-number>", "<time-value>");

        auto port = from_chars<std::uint16_t>(argv[1]);
        if (!port || !*port)
            logger{} << "Port must be in [1;65535]";

        std::optional<unsigned> num;
        num = from_chars<int>(argv[2]);
        if (!num || !*num)
            logger{} << "Number of client must be positive";

        std::optional<unsigned> threads;
        threads = from_chars<int>(argv[3]);
        if (!threads || !*threads)
            logger{} << "Number of threads must be positive";

        auto time = from_chars<int>(argv[4]);
        if (!time || !*time)
            logger{} << "Value of time must be positive";

        boost::asio::io_context ctx;
        boost::asio::signal_set stop_signals{ctx, SIGINT, SIGTERM};
        boost::asio::deadline_timer stop_times{ctx, boost::posix_time::seconds(*time)};

        std::string request;
        request = 'h';
        request.resize(1048576, 'h');
        double counter = 0;

        //locale ip to connect
        std::string ip = "127.0.0.1";
        int port_ip = 8888;

        boost::asio::ip::tcp::endpoint ep{
                boost::asio::ip::tcp::v4(), *port};
        boost::asio::ip::tcp::endpoint way(
                boost::asio::ip::address::from_string(ip), port_ip);

        auto start = std::chrono::high_resolution_clock::now();

        stop_signals.async_wait([&, start](boost::system::error_code ec, int /*signal*/) {
            if (ec)
                return;
            logger{} << "Terminating in response to signal.";
            auto end = std::chrono::high_resolution_clock::now();
            auto time_span =
                    std::chrono::duration<double, std::milli>(end - start);
            std::cout << "Time: " << time_span.count() << "\n";
            ctx.stop();
        });

        stop_times.async_wait([&, counter](boost::system::error_code ec) {
            if (ec)
                return;
            logger{} << "Terminating in response to time.\n" << "Counter: " << counter << "\n";
            ctx.stop();
        });

        for (;;) {
            for (auto i = 0; i < num; ++i) {

                boost::system::error_code ec;
                boost::asio::ip::tcp::socket socket_new{make_strand(ctx)};

                /*mtosdadp::spawn(make_strand(ctx),bind_executor(
                         ctx, [socket = boost::beast::tcp_stream{std::move(socket_new)},
                                 ep, way, request, ec](auto yc) mutable {*/

                bccoro::spawn(bind_executor(
                        ctx, [socket = boost::beast::tcp_stream{std::move(socket_new)},
                                ep, way, request, ec, i](bccoro::yield_context yc) mutable {

                            constexpr static std::chrono::seconds timeout{10};
                            constexpr static std::chrono::seconds timeout_close{1};

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
                                return;
                            }

                            socket.expires_after(timeout);
                            socks5::client::reply_first socks_reply_first;
                            auto n = socket.async_read_some(socks_reply_first.buffers(), yc[ec]);

                            if (ec) {
                                if (ec != boost::asio::error::operation_aborted &&
                                    (ec != boost::asio::error::eof || n))
                                    logger{} << "First reply error: " << ec.message();
                                return;
                            }

                            socket.expires_after(timeout);
                            socks5::client::request_second socks_request_second(
                                    socks5::client::request_second::connect, way);
                            socket.async_write_some(socks_request_second.buffers(), yc[ec]);

                            if (ec) {
                                if (ec != boost::asio::error::operation_aborted)
                                    logger{} << "Failed to write second request: "
                                             << ec.message();
                                return;
                            }

                            socket.expires_after(timeout);
                            socks5::client::reply_second socks_reply_second;
                            auto r = socket.async_read_some(socks_reply_second.buffers(), yc[ec]);

                            if (ec) {
                                if (ec != boost::asio::error::operation_aborted &&
                                    (ec != boost::asio::error::eof || r))
                                    logger{} << "Second reply error: " << ec.message();
                                return;
                            }


                            socket.expires_after(timeout);
                            socket.async_write_some(boost::asio::buffer(request), yc[ec]);

                            if (ec) {
                                if (ec != boost::asio::error::operation_aborted)
                                    logger{} << "Failed to write second request: "
                                             << ec.message();
                                return;
                            }

                            std::array<char, 256> response{};
                            socket.expires_after(timeout);

                            while (std::size_t s = socket.async_read_some(boost::asio::buffer(response), yc[ec])) {
                                std::cout.write(response.data(), s);
                                if (ec) {
                                    if (ec != boost::asio::error::operation_aborted &&
                                        (ec != boost::asio::error::eof || s))
                                        logger{} << "Failed to read: " << ec.message();
                                    return;
                                }
                            }
                            std::cout << i << " - Client\n";
                            socket.expires_after(timeout_close);
                        }));
            }
        }
        std::vector<std::thread> workers;
        std::size_t extra_workers = *threads - 1;
        workers.reserve(extra_workers);
        auto ase = at_scope_exit([&]() noexcept {
            for (auto &t:workers) {
                t.join();
            }
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
