//
// Created by Артем Католиков on 20.12.2019.
//

#ifndef SOCKS5_LIBUV_SESSION_HPP
#define SOCKS5_LIBUV_SESSION_HPP

#include <uv.h>
#include <map>
#include <queue>
#include <memory>

class Session
{
public:
    Session()= default;
    ~Session()= default;
    std::shared_ptr<uv_tcp_t> connection;
};

#endif //SOCKS5_LIBUV_SESSION_HPP
