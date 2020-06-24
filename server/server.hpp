
#ifndef SOCKS5_LIBUV_SERVER_HPP
#define SOCKS5_LIBUV_SERVER_HPP

#include <uv.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include "session.hpp"
#include "msg.h"

typedef std::map<uv_stream_t*, Session> session_map_t;
typedef std::map<uv_stream_t*, Session> socket_map_t_server;
typedef std::map<uv_stream_t*, uv_stream_t*> socket_map_t_client;
typedef std::map<uv_stream_t*, Session> socket_map_t_client_server;

class Server{

public:

    Server();

    ~Server();

    void proxy_start(uv_stream_t *client);

    static Server* get_instance();

    void transform_ip_port(std::string msg);

    bool init(const std::string& in_person_addr, int in_port);

    void on_new_connection(uv_stream_t *server, int status);

    static bool s5_parse_auth(std::string msg);

    bool s5_parse_req(std::string msg);

    void on_server_conn(uv_connect_t *req, int status);

    void on_client_read(uv_stream_t *client, ssize_t len, const uv_buf_t *buf);

    void on_server_read(uv_stream_t *server, ssize_t len, const uv_buf_t *buf);

    void on_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf);

    void second_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf);

    void write(const std::string& message_one, uv_stream_t *client);

    void write_after_auth(const std::string& message_second, uv_stream_t *client);

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

    std::unique_ptr<uv_tcp_t> m_server;
    std::unique_ptr<uv_loop_t> loop;
    std::unique_ptr<uv_connect_t> m_server_req;

    Session sock_session;

    Session new_session;

    std::unique_ptr<uv_mutex_t> mutex;

    session_map_t open_sessions;
    socket_map_t_server open_socket;
    socket_map_t_client client_socket;
    socket_map_t_client_server server_socket;

    struct sockaddr_in m_addr;
    struct sockaddr_in req_addr;
};
#endif //SOCKS5_LIBUV_SERVER_HPP
