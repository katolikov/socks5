#ifndef SOCKS5_HPP_CLIENT
#define SOCKS5_HPP_CLIENT

#include <array>
#include <string>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace socks5 {

    const unsigned char version = 0x05;

    class request_first {
    public:
        const unsigned char num = 0x01;
        const unsigned char type = 0x00;

        request_first() : version_(version),
                          auth_num_(num),
                          auth_(type) {}


        std::array<boost::asio::const_buffer, 3> buffers() const {
            return
                    {
                            {
                                    boost::asio::buffer(&version_, 1),
                                    boost::asio::buffer(&auth_num_, 1),
                                    boost::asio::buffer(&auth_, 1)
                            }
                    };
        }

    private:
        unsigned char version_;
        unsigned char auth_num_;
        unsigned char auth_;
    };

    class request_second {
    public:
        const unsigned char addr_type = 0x01;

        enum command_type {
            connect = 0x01,
        };

        request_second(command_type cmd, const boost::asio::ip::tcp::endpoint &endpoint)
                : version_(version),
                  command_(cmd),
                  null_byte_(0),
                  type_address_(addr_type) {
            if (endpoint.protocol() != boost::asio::ip::tcp::v4()) {
                throw boost::system::system_error(
                        boost::asio::error::address_family_not_supported);
            }

            unsigned short port = endpoint.port();
            port_high_byte_ = (port >> 8) & 0xff;

            // Save IP address in network byte order.
            address_ = endpoint.address().to_v4().to_bytes();
        }

        std::array<boost::asio::const_buffer, 10> buffers() const {
            return
                    {
                            {
                                    boost::asio::buffer(&version_, 1),
                                    boost::asio::buffer(&command_, 1),
                                    boost::asio::buffer(&null_byte_, 1),
                                    boost::asio::buffer(&type_address_, 1),
                                    boost::asio::buffer(address_, 4),
                                    boost::asio::buffer(&port_high_byte_, 2)
                            }
                    };
        }

    private:
        unsigned char version_;
        unsigned char command_;
        unsigned char null_byte_;
        unsigned char type_address_;
        boost::asio::ip::address_v4::bytes_type address_;
        unsigned char port_high_byte_;
    };

    class reply_first {
    public:
        enum auth_ {
            no_auth = 0x00,
        };

        reply_first()
                : version_(),
                  status_() {
        }

        std::array<boost::asio::mutable_buffer, 2> buffers() {
            return
                    {
                            {
                                    boost::asio::buffer(&version_, 1),
                                    boost::asio::buffer(&status_, 1),
                            }
                    };
        }

        bool success() const {
            return status_ == no_auth;
        }

        unsigned char status() const {
            return status_;
        }

    private:
        unsigned char version_;
        unsigned char status_;
    };

    class reply_second {
    public:
        enum status_type {
            request_granted = 0x00,
            request_failed = 0x01,
        };

        reply_second()
                : version_(),
                  status_(),
                  null_byte_(0),
                  type_addr_() {
        }

        std::array<boost::asio::mutable_buffer, 10> buffers() {
            return
                    {
                            {
                                    boost::asio::buffer(&version_, 1),
                                    boost::asio::buffer(&status_, 1),
                                    boost::asio::buffer(&null_byte_, 1),
                                    boost::asio::buffer(&type_addr_, 1),
                                    boost::asio::buffer(address_, 4),
                                    boost::asio::buffer(&port_high_byte_, 2),

                            }
                    };
        }

        bool success() const {
            return null_byte_ == 0 && status_ == request_granted;
        }

        unsigned char status() const {
            return status_;
        }

        boost::asio::ip::tcp::endpoint endpoint() const {
            unsigned short port = port_high_byte_;
            port = (port << 8) & 0xff00;

            boost::asio::ip::address_v4 address(address_);

            return boost::asio::ip::tcp::endpoint(address, port);
        }

    private:
        unsigned char version_;
        unsigned char status_;
        unsigned char null_byte_;
        unsigned char type_addr_;
        boost::asio::ip::address_v4::bytes_type address_;
        unsigned char port_high_byte_;
    };

} // namespace socks5

#endif // SOCKS5_HPP
