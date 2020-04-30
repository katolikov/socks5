//
// Created by Артем Католиков on 21.12.2019.
//
#include "proxy5.hpp"

Server::~Server(){

    std::cout << "\033[1;5;31mProxy terminated\033[0m\n";
    uv_loop_close(loop);
    delete client_req;
    delete server_req;
    delete loop;
}

Server* Server::get_instance(){

    static Server server;
    return &server;
}

void Server::alloc_client_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {

    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void Server::alloc_server_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {

    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

int Server::init(const std::string& in_person_addr, int in_port){

    uv_tcp_init(loop, &m_server);

    uv_ip4_addr((char*)in_person_addr.c_str(), in_port, &m_addr);

    uv_tcp_bind(&m_server, (const struct sockaddr*)&m_addr, 0);

    error = uv_listen((uv_stream_t*)&m_server, 0,[](uv_stream_t* server, int status){
        Server::get_instance()->on_new_connection(server, status);});

    if (error == 0) {
        std::cout << "\033[1;5;33m[INFO]\033[0m Proxy start: " << (char*)in_person_addr.c_str() << ":" << in_port << '\n';
        return uv_run(loop, UV_RUN_DEFAULT);

    }
    else{
        std::cout << "\033[1;5;31m" << "Listen error: "<< uv_strerror(error) << "\033[m\n";
        uv_close((uv_handle_t*)&m_server, nullptr);
        return 1;

    }
}

void Server::on_server_msg(uv_stream_t *server, ssize_t i, const uv_buf_t *buf) {

    auto connection_pos = open_sessions.find(server);
    if (connection_pos != open_sessions.end()) {
        if (i == UV_EOF) {
            std::cout << "\033[1;5;31mClient Disconnected\n\033[0m";
            delete[] buf->base;
        } else if (i > 0) {
            std::cout << "\033[1;5;34mProxy working server\n\033[0m";
            uv_buf_t buf_server = uv_buf_init(buf->base, i);
            error = uv_write(&m_server_wreq, (uv_stream_t *)client_get, &buf_server, 1, nullptr);
            std::cout << error;
        }
    }
}

void Server::on_server_conn(uv_connect_t *req, int status) {

    if (status < 0) {
        std::cout << "\033[1;5;31m" << "New server connecting error: " << uv_strerror(status) << "\033[0m\n";
    }

    std::cout << "\n\033[1;7;34mPROXY.\n\033[7;0m";

    error = uv_read_start((uv_stream_t *) req->handle,
                          [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                              Server::get_instance()->alloc_server_buffer(stream, size, buf);
                          },
                          [](uv_stream_t *client, ssize_t i, const uv_buf_t *buf) {
                              if (i < 0)
                                  std::cout << "\033[1;5;31m" << uv_err_name(i) << ": PROXY_SERVER_ERROR\n\033[0m";
                              Server::get_instance()->on_server_msg(client, i, buf);
                          });
    if (error == 0) {
        std::cout << "\033[1;5;34mRead to server start!\n\033[0m";
    } else {
        std::cout << "\033[1;5;31mRead to server: " << uv_strerror(error) << "\033[0m\n";
    }
}

void Server::proxy_start(uv_stream_t* client) {

    sock_session.connection = std::make_shared<uv_tcp_t>();

    uv_tcp_init(loop, sock_session.connection.get());

    server_get = sock_session.connection.get();
    uv_ip4_addr((char *) ip_req.c_str(), port, &req_addr);

    error = uv_tcp_connect(&m_server_req, sock_session.connection.get(), (const struct sockaddr *) &req_addr,  [](uv_connect_t *req, int status) {
        Server::get_instance()->on_server_conn(req, status);
    });

    if (error == 0) {
        std::cout << "\033[1;5;34mConnecting to: " << ip_req << ':' << port << "\033[0m\n";
        auto *key = (uv_stream_t *) sock_session.connection.get();
        open_socket.insert({key, sock_session});
    } else {
        std::cout << "\033[1;5;31mConnecting to ip error: " << uv_strerror(error) << "\033[0m\n";
        uv_close((uv_handle_t *) sock_session.connection.get(), nullptr);
        after_auth_answer[1] = 0x03;
    }
}

void Server::transform_ip_port(std::string msg){

    char m_ip[20];

    snprintf(m_ip, 17, "%u.%u.%u.%u",
             (unsigned int)((unsigned char)msg[4]),
             (unsigned int)((unsigned char)msg[5]),
             (unsigned int)((unsigned char)msg[6]),
             (unsigned int)((unsigned char)msg[7]));

    ip_req = m_ip;

    int s = (int)((unsigned char)msg[8]);

    if(s > 0){
        port = (256 * s) + (int)((unsigned char)msg[9]);
    } else{
        port = (int)((unsigned char)msg[9]);
    }
}


bool Server::s5_parse_auth(std::string msg){

    return !((msg[0] != 0x05) && (msg[2] != 0x00));
}

bool Server::s5_parse_req(std::string msg){

    if((msg[0]== 0x05) && (msg[1]== 0x01) && (msg[3] == 0x01)){

        Server::transform_ip_port(msg);

        std::cout << "\033[1;5;34mIP: " << ip_req << "\033[0m\n";
        std::cout << "\033[1;5;34mPORT: " << port << "\033[0m\n";

        return true;
    } else{
        after_auth_answer[1] = 0x07;
        return false;
    }
}

void Server::write(const std::string& message_one, uv_stream_t *client){

    int len = (int)message_one.length();

    char buffer[len];

    uv_buf_t buf = uv_buf_init(buffer, sizeof(buffer));

    buf.len = len;
    buf.base = (char*)message_one.c_str();

    int buf_count = 1;

    error = uv_write(&m_write_req, client, &buf, buf_count, nullptr);

    if (error == 0) {
        std::cout << "\033[1;5;34mSend to client succeed!\n\033[0m";
    }else{
        std::cout << "\033[1;5;31mSend to client error: "<< uv_strerror(error) << "\033[0m\n";
    }
}


void Server::on_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf) {

    auto connection_pos = open_sessions.find(client);

    if (connection_pos != open_sessions.end()) {
        if (i == UV_EOF) {
            std::cout << "\033[1;5;31mClient Disconnected\n\033[0m";
            delete[] buf->base;
        } else if (i > 0) {

            std::string str(buf->base, buf->len);

            if(h == 0){
                h++;
            }

            if(h == 1) {
                if (Server::s5_parse_auth(str)) {
                    h++;
                    str.clear();
                    Server::write(auth_answer, client);
                } else {
                    str.clear();
                    std::cout << "\033[1;5;31mUnknown protocol\n\033[0m";
                    h--;
                    delete[] buf->base;
                }
            } else if(h == 2) {
                if (Server::s5_parse_req(str)) {
                    after_auth_answer[1] = 0x00;
                    str.clear();
                    h++;
                    Server::proxy_start(client);

                    Server::write(after_auth_answer, client);

                    delete[] buf->base;
                } else {
                    str.clear();
                    std::cout << "\033[1;5;31mUnknown message\n\033[0m";
                    after_auth_answer[1] = 0x08;
                    Server::write(after_auth_answer, client);
                    h -= 2;
                    delete[] buf->base;
                }
            } else if(h == 3){
                uv_read_stop(client);
                uv_buf_t buf_server = uv_buf_init(buf->base, i);
                error = uv_write(&m_server_wreq, (uv_stream_t*)server_get, &buf_server, 1, [](uv_write_t *req, int status){
                    Server::get_instance()->on_write_cb(req, status);
                });
            }
        }
    }
}

void Server::on_write_cb(uv_write_t * req, int status) {
    if (status < 0) {
        std::cout << "\033[1;5;31m" << "New server connecting error: " << uv_strerror(status) << "\033[0m\n";
    }

    error = uv_read_start((uv_stream_t *) req->handle,
                          [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                              Server::get_instance()->alloc_client_buffer(stream, size, buf);
                          },
                          nullptr);
    if (error == 0) {
        std::cout << "\033[1;5;34mRead to server start!\n\033[0m";
    } else {
        std::cout << "\033[1;5;31mRead to server: " << uv_strerror(error) << "\033[0m\n";
    }
}

void Server::on_new_connection(uv_stream_t *server, int status) {

    if (status < 0) {
        std::cout << "\033[1;5;31mNew connection error: \033[0m" << uv_strerror(status) << '\n';
    }

    new_session.connection = std::make_shared<uv_tcp_t>();

    uv_tcp_init(loop, new_session.connection.get());

    client_get = new_session.connection.get();

    if (uv_accept(server, (uv_stream_t *) new_session.connection.get()) == 0) {
        std::cout << "\033[1;5;33m[INFO] \033[1;5;39mNew connection.\n\033[0m";
        uv_read_start((uv_stream_t *) new_session.connection.get(),
                      [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                          Server::get_instance()->alloc_client_buffer(stream, size, buf);
                      },
                      [](uv_stream_t *stream, ssize_t i, const uv_buf_t *buf) {
                          if (i < 0) std::cout << "\033[1;5;31m" << uv_err_name(i) << ": FIRST_SERVER_ERROR\n\033[0m";
                          Server::get_instance()->on_msg_recv(stream, i, buf);
                      });

        auto *key = (uv_stream_t *) new_session.connection.get();
        open_sessions.insert({key, new_session});
    } else {
        uv_close((uv_handle_t *) new_session.connection.get(), nullptr);
    }
}