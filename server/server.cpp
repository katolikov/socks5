#include "server.hpp"
Server::Server(){
}

Server::~Server(){
    std::cout << "\033[1;5;31mProxy terminated\033[0m\n";
    uv_loop_close(uv_default_loop());
}

Server* Server::get_instance(){

    static Server server;
    return &server;
}

bool Server::init(const std::string& in_person_addr, int in_port) {

    error = uv_tcp_init(uv_default_loop(), &m_server);

    if(error != 0){
        std::cout << "\033[1;5;31m" << "Init error: "
                  << uv_strerror(error) << "\033[m\n";
    }

    error = uv_ip4_addr((char *) in_person_addr.c_str(), in_port, &m_addr);

    if (error != 0) {
        std::cout << "\033[1;5;31m" << "uv_ip4_addr error: "
                  << uv_strerror(error) << "\033[m\n";
    }

    error = uv_tcp_bind(&m_server,
                        reinterpret_cast<const struct sockaddr *> (&m_addr), 0);

    if (error != 0) {
        std::cout << "\033[1;5;31m" << "Bind error: "
                  << uv_strerror(error) << "\033[m\n";
    }

    error = uv_listen(reinterpret_cast<uv_stream_t *> (&m_server), 0,
                      [](uv_stream_t *server, int status) {
                          Server::get_instance()->on_new_connection(server, status);
                      });

    if (error == 0) {
        std::cout << "\033[1;5;33m[INFO]\033[0m Proxy start: "
                  << (char *) in_person_addr.c_str() << ":" << in_port
                  << '\n';
        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        return true;

    } else {
        std::cout << "\033[1;5;31m" << "Listen error: "
                  << uv_strerror(error) << "\033[m\n";
        uv_close(reinterpret_cast<uv_handle_t *> (&m_server), nullptr);
        return false;

    }
}

void Server::alloc_server(
        uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {

    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

void Server::alloc_client_buffer(
        uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {

    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

void Server::alloc_server_buffer(
        uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {

    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

void Server::on_client_read(
        uv_stream_t *client, ssize_t len, const uv_buf_t *buf) {

    auto connection_cl = open_sessions.find(client);
    Session get_client = connection_cl->second;
    Session get_server = open_socket.begin()->second;

    if (connection_cl != open_sessions.end()) {
        if (len < 0) {
            std::cout << "\033[1;5;31m" << uv_strerror(len)
                      << ": Read client error\n\033[0m";
            uv_close(reinterpret_cast<uv_handle_t *> (
                             get_client.connection.get()), nullptr);
        } else if (len > 0) {
            std::cout << "\033[1;5;34mProxy working client\n\033[0m";

            uv_buf_t buf_server = uv_buf_init(buf->base, len);
            uv_write_t * m_server_wreq = (uv_write_t *) malloc(sizeof(uv_write_t));

            error = uv_write(m_server_wreq,
              reinterpret_cast<uv_stream_t *> (get_server.connection.get()),
                &buf_server, 1, [](uv_write_t *req, int status){
                  free(req);
                });

            if (error == 0) {
                std::cout << "\033[1;5;34mWrite to server succeed!\n\033[0m";
            } else {
                std::cout << "\033[1;5;31mWrite to server error: "
                          << uv_strerror(error) << "\033[0m\n";
            }

        }
    }
}

void Server::on_server_read(uv_stream_t *server, ssize_t len, const uv_buf_t *buf) {

    auto connection_pos = open_socket.find(server);
    auto connection_cl = client_socket.find(server);
    Session get_server= connection_pos->second;

    if (connection_pos != open_socket.end()) {
        if (len < 0) {
            if (len == UV_EOF) {
                std::cout << "\033[1;5;31m" << "Client error: "
                          << uv_strerror(len) << "\033[0m\n";
                uv_close(reinterpret_cast<uv_handle_t *>(
                                 get_server.connection.get()), nullptr);
            } else {
                std::cout << "\033[1;5;31m"
                          << uv_strerror(len) << ": Read server error\n\033[0m";
                uv_close(reinterpret_cast<uv_handle_t *>(
                                 get_server.connection.get()), nullptr);
            }
        } else if (len > 0) {
            std::cout << "\033[1;5;34mProxy working server\n\033[0m";

            uv_buf_t buf_server = uv_buf_init(buf->base, len);
            uv_write_t * m_client_wreq = (uv_write_t *) malloc(sizeof(uv_write_t));

            error = uv_write(m_client_wreq,
                  connection_cl->second,
                  &buf_server, 1, [](uv_write_t *req, int status){
                    free(req);
                  });

            if (error == 0) {
                std::cout << "\033[1;5;34mWrite to client succeed!\n\033[0m";
            } else {
                std::cout << "\033[1;5;31mWrite to client error: "
                          << uv_strerror(error) << "\033[0m\n";
            }

        }
    }
}

void Server::on_server_conn(uv_connect_t *req, int status) {

    if (status < 0) {
        std::cout << "\033[1;5;31m" << "New server connecting error: "
                  << uv_strerror(status) << "\033[0m\n";
    }

    std::cout << "\n\033[1;7;34mPROXY.\n\033[7;0m";

    auto connection_pos = open_socket.find(
                    reinterpret_cast<uv_stream_t*>(req->handle));

    auto connection_cl = client_socket.find(
                    reinterpret_cast<uv_stream_t*>(req->handle));

    Session get = connection_pos->second;

    if (connection_pos != open_socket.end()) {

        error = uv_read_start(connection_cl->second,
                              [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                                  Server::get_instance()->alloc_client_buffer(stream, size, buf);
                              },
                              [](uv_stream_t *client, ssize_t len, const uv_buf_t *buf) {
                                  Server::get_instance()->on_client_read(client, len, buf);
                              });

        if (error == 0) {
            std::cout << "\033[1;5;34mRead to client start!\n\033[0m";
        } else {
            std::cout << "\033[1;5;31mRead to client error: "
                      << uv_strerror(error) << "\033[0m\n";
        }

        error = uv_read_start(reinterpret_cast<uv_stream_t *>(
                                      get.connection.get()),
                              [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                                  Server::get_instance()->alloc_server_buffer(stream, size, buf);
                              },
                              [](uv_stream_t *server, ssize_t len, const uv_buf_t *buf) {
                                  Server::get_instance()->on_server_read(server, len, buf);
                              });

        if (error == 0) {
            std::cout << "\033[1;5;34mRead to server start!\n\033[0m";
        } else {
            std::cout << "\033[1;5;31mRead to server: " <<
                      uv_strerror(error) << "\033[0m\n";
        }
    }
}

void Server::proxy_start(uv_stream_t *client) {

    sock_session.connection = std::make_shared<uv_tcp_t>();

    error = uv_tcp_init(uv_default_loop(), sock_session.connection.get());

    if (error == 0) {

        error = uv_ip4_addr((char *) ip_req.c_str(), port, &req_addr);

        if (error != 0) {
            std::cout << "\033[1;5;31mIP4 addr libuv error: "
                      << uv_strerror(error) << "\033[0m\n";
        }

        error = uv_tcp_connect(&m_server_req, sock_session.connection.get(),
                               reinterpret_cast<const struct sockaddr *> (&req_addr),
                               [](uv_connect_t *req, int status) {
                                   Server::get_instance()->on_server_conn(req, status);
                               });

        if (error == 0) {
            std::cout << "\033[1;5;34mConnecting to: " <<
                      ip_req << ':' << port << "\033[0m\n";
            auto *key = reinterpret_cast<uv_stream_t *>(
                    sock_session.connection.get());

            open_socket.insert({key, sock_session});

            client_socket.insert({{key, client}});

        } else {
            std::cout << "\033[1;5;31mConnecting to ip error: "
                      << uv_strerror(error) << "\033[0m\n";
            uv_close(reinterpret_cast<uv_handle_t *> (
                             sock_session.connection.get()), nullptr);
            after_auth_answer[1] = 0x03;
        }
    } else {
        std::cout << "\033[1;5;31mConnecting to ip error: "
                  << uv_strerror(error) << "\033[0m\n";
    }
}

void Server::transform_ip_port(std::string msg) {

    char m_ip[20];

    snprintf(m_ip, 17, "%u.%u.%u.%u",
             static_cast<unsigned int>((static_cast<unsigned char>(msg[4]))),
             static_cast<unsigned int>((static_cast<unsigned char>(msg[5]))),
             static_cast<unsigned int>((static_cast<unsigned char>(msg[6]))),
             static_cast<unsigned int>((static_cast<unsigned char>(msg[7]))));

    ip_req = m_ip;

    int s = static_cast<int>((static_cast<unsigned char>(msg[8])));

    if (s > 0) {
        port = (256 * s) + static_cast<int>((
                static_cast<unsigned char>(msg[9])));
    } else {
        port = static_cast<int>((static_cast<unsigned char>(msg[9])));
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

void Server::write_after_auth(
        const std::string &message_second, uv_stream_t *client) {

    Session get_client;
    auto connection_pos = open_sessions.find(client);
    get_client = connection_pos->second;

    int len = static_cast<int> (message_second.length());

    char buffer[len];

    uv_buf_t buf = uv_buf_init(buffer, len);

    buf.len = len;
    buf.base = (char *) message_second.c_str();

    int buf_count = 1;

    if (after_auth_answer[1] == 0x00) {

        Server::proxy_start(reinterpret_cast<uv_stream_t *>(
                                    get_client.connection.get()));
        ip_req.clear();
    }

    uv_write_t * m_write_req = (uv_write_t *) malloc(sizeof(uv_write_t));

    error = uv_write(m_write_req, reinterpret_cast<uv_stream_t *>(
                             get_client.connection.get()),
                     &buf, buf_count, [](uv_write_t *req, int status){
                       free(req);
                     });

    if (error == 0) {
        std::cout << "\033[1;5;34mSend to client succeed!\033[0m\n";
    } else {
        std::cout << "\033[1;5;31mSend to client error: "
                  << uv_strerror(error) << "\033[0m\n";
    }
}

void Server::write(const std::string& message_one, uv_stream_t *client) {

    Session get_client;
    auto connection_pos = open_sessions.find(client);
    get_client = connection_pos ->second;

    int len = static_cast<int> (message_one.length());

    char buffer[len];

    uv_buf_t buf = uv_buf_init(buffer, len);

    buf.len = len;
    buf.base = (char *) message_one.c_str();

    int buf_count = 1;

    uv_write_t * m_write_req = (uv_write_t *) malloc(sizeof(uv_write_t));

    error = uv_write(m_write_req, reinterpret_cast<uv_stream_t *>(
                             get_client.connection.get()),
                     &buf, buf_count, [](uv_write_t *req, int status){
                       free(req);
                     });

    if (error == 0) {
        std::cout << "\033[1;5;34mSend to client succeed!\n\033[0m";
    } else {
        std::cout << "\033[1;5;31mSend to client error: "
                  << uv_strerror(error) << "\033[0m\n";
    }

    error = uv_read_start(
            reinterpret_cast<uv_stream_t *>(get_client.connection.get()),
            [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                Server::get_instance()->alloc_server(stream, size, buf);
            },
            [](uv_stream_t *stream, ssize_t i, const uv_buf_t *buf) {
                if (i < 0)
                    std::cout << "\033[1;5;31m" <<
                              uv_strerror(i) << ": Read server error\n\033[0m";
                Server::get_instance()->second_msg_recv(stream, i, buf);
            });

    if (error == 0) {
        std::cout << "\033[1;5;34mRead to server start!\n\033[0m";
    } else {
        std::cout << "\033[1;5;31mRead to server: " << uv_strerror(error) << "\033[0m\n";
    }
}

void Server::second_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf) {

    Session get_client;
    auto connection_pos = open_sessions.find(client);
    get_client = connection_pos ->second;

    if (connection_pos != open_sessions.end()) {
        if (i == UV_EOF) {
            std::cout << "\033[1;5;31mClient Disconnected\n\033[0m";

        } else if (i > 0) {

            uv_read_stop(
                    reinterpret_cast<uv_stream_t *>(get_client.connection.get()));

            std::string str(buf->base, i);

            if (Server::s5_parse_req(str)) {

                after_auth_answer[1] = 0x00;
                Server::write_after_auth(after_auth_answer,
                                         reinterpret_cast<uv_stream_t *>(get_client.connection.get()));

            } else {
                std::cout << "\033[1;5;31mUnknown message\n\033[0m";
                after_auth_answer[1] = 0x08;
                Server::write_after_auth(after_auth_answer,
                                         reinterpret_cast<uv_stream_t *>(get_client.connection.get()));

            }
        }
    }
}

void Server::on_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf) {

    Session get_client;
    auto connection_pos = open_sessions.find(client);
    get_client = connection_pos ->second;

    if (connection_pos != open_sessions.end()) {
        if (i == UV_EOF) {
            std::cout << "\033[1;5;31mClient Disconnected\n\033[0m";

        } else if (i > 0) {

            uv_read_stop(
                    reinterpret_cast<uv_stream_t *>(get_client.connection.get()));

            std::string str(buf->base, i);

            if (Server::s5_parse_auth(str)) {

                Server::write(auth_answer,
                              reinterpret_cast<uv_stream_t *>(get_client.connection.get()));
            } else {
                std::cout << "\033[1;5;31mUnknown protocol\n\033[0m";

            }
        }
    }
}

void Server::on_new_connection(uv_stream_t *server, int status) {

    if (status < 0) {
        std::cout << "\033[1;5;31mNew connection error: \033[0m"
                  << uv_strerror(status) << '\n';
    }

    new_session.connection = std::make_shared<uv_tcp_t>();

    error = uv_tcp_init(uv_default_loop(), new_session.connection.get());

    if (error == 0) {
        if (uv_accept(server,
                      reinterpret_cast<uv_stream_t *>(new_session.connection.get())) == 0) {

            std::cout << "\033[1;5;33m[INFO] \033[1;5;39mNew connection.\n\033[0m";
            uv_read_start(reinterpret_cast<uv_stream_t *>(new_session.connection.get()),
                          [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                              Server::get_instance()->alloc_server(stream, size, buf);
                          },
                          [](uv_stream_t *stream, ssize_t i, const uv_buf_t *buf) {
                              if (i < 0)
                                  std::cout << "\033[1;5;31m" <<
                                            uv_strerror(i) << ": FIRST_SERVER_ERROR\n\033[0m";
                              Server::get_instance()->on_msg_recv(stream, i, buf);
                          });

            auto *key = reinterpret_cast<uv_stream_t *>(new_session.connection.get());
            open_sessions.insert({key, new_session});
        } else {
            uv_close(
                    reinterpret_cast<uv_handle_t *>(new_session.connection.get()), nullptr);
        }
    } else {
        std::cout << "\033[1;5;31m"
                  << uv_strerror(error) << ": ERROR INIT\n\033[0m";
    }
}
