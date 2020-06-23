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
    Session();
    ~Session()= default;

    void Activate()
    {
        active = true;
    }

    void Deactivate()
    {
        active = false;
    }

    bool IsActive() const
    {
        return active;
    }

    bool active;

    std::shared_ptr<uv_tcp_t> connection;
    std::shared_ptr<uv_timer_t> activity_timer;
};

#endif //SOCKS5_LIBUV_SESSION_HPP
