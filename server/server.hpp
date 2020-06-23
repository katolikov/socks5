
#ifndef SOCKS5_LIBUV_SERVER_HPP
#define SOCKS5_LIBUV_SERVER_HPP

#include <uv.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include "session.hpp"
#include "msg.h"

typedef std::map<uv_stream_t*, Session> session_map_t;
typedef std::map<uv_stream_t*, Session> socket_map_t;
typedef std::map<uv_timer_t*, uv_stream_t*> timer_map_t;

class Server{

public:

    Server();

    ~Server();

    void proxy_start();

    static Server* get_instance();

    void transform_ip_port(std::string msg);

    bool init(const std::string& in_person_addr, int in_port);

    void on_new_connection(uv_stream_t *server, int status);

    void on_client_timeout(uv_timer_t* handle);

    void remove_client(uv_stream_t* client);

    static bool s5_parse_auth(std::string msg);

    bool s5_parse_req(std::string msg);

    void on_server_conn(uv_connect_t *req, int status);

    void on_client_read(uv_stream_t *client, ssize_t len, const uv_buf_t *buf);

    void on_server_read(uv_stream_t *server, ssize_t len, const uv_buf_t *buf);

    void on_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf);

    void second_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf);

    void write(const std::string& message_one, uv_stream_t *client);

    void write_after_auth(const std::string& message_second, uv_stream_t *client);

    msg_buffer* get_read_buffer()
    {
        return &read_buffer;
    }

    static void alloc_client_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

    static void alloc_server_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

    static void alloc_server(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

private:

    char auth_answer[2] =
		{ 0x05, 0x00 };

    char after_auth_answer[11] =
		   { 0x05, 0x00, 0x00, 0x01, 0x00,
          	0x00, 0x00, 0x00, 0x00, 0x00,
          	0x00 };

    std::string ip_req;

    int error;
    int port;

    uv_write_t m_server_wreq;
    uv_write_t m_client_wreq;

    uv_tcp_t m_server;

    uv_tcp_t* server_get;

    uv_stream_t* client_req;

    uv_connect_t m_server_req;

    uv_loop_t *loop;

    uv_write_t m_write_req;

    msg_buffer msg_queue;

    msg_buffer read_buffer;

    timer_map_t active_timers;

    Session sock_session;
    Session new_session;

    session_map_t open_sessions;
    socket_map_t open_socket;

    struct sockaddr_in m_addr;
    struct sockaddr_in req_addr;
};
#endif //SOCKS5_LIBUV_SERVER_HPP
