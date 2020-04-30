//
// Created by Артем Католиков on 21.12.2019.
//

#ifndef SOCKS5_LIBUV_PROXY5_HPP
#define SOCKS5_LIBUV_PROXY5_HPP

#include <uv.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include "session.hpp"

typedef std::map<uv_stream_t*, Session> session_map_t;
typedef std::map<uv_stream_t*, Session> socket_map_t;

class Server{

public:

    Server()= default;

    ~Server();

    void proxy_start(uv_stream_t* client);

    static Server* get_instance();

    void transform_ip_port(std::string msg);

    int init(const std::string& in_person_addr, int in_port);

    void on_new_connection(uv_stream_t *server, int status);

    static bool s5_parse_auth(std::string msg);

    bool s5_parse_req(std::string msg);

    void on_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf);

    void on_server_conn(uv_connect_t *req, int status);

    void on_write_cb(uv_write_t *req, int status);

    void write(const std::string& message_one, uv_stream_t *client);

    void on_server_msg(uv_stream_t *server, ssize_t i, const uv_buf_t *buf);

    static void alloc_client_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

    static void alloc_server_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);


private:

    std::string auth_answer =
            { 0x05, 0x00 };

    std::string after_auth_answer =
            { 0x05, 0x00, 0x00, 0x01, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00,
              0x00 };

    std::string ip_req;

    int error;
    int port;

    //uv_loop_t *loop;
    uv_loop_t* loop = uv_default_loop();

    uv_write_t m_client_wreq;
    uv_write_t m_server_wreq;

    uv_tcp_t m_server;

    uv_tcp_t *client_get = new uv_tcp_t();
    uv_tcp_t *server_get = new uv_tcp_t();

    uv_stream_t *client_req = new uv_stream_t();
    uv_stream_t *server_req = new uv_stream_t();

    uv_connect_t m_server_req;

    uv_write_t m_write_req;

    Session sock_session;
    Session new_session;

    session_map_t open_sessions;
    socket_map_t open_socket;

    struct sockaddr_in m_addr;
    struct sockaddr_in req_addr;

    int h = 0;
};

#endif //SOCKS5_LIBUV_PROXY5_HPP
