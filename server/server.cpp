#include "server.hpp"
Server::Server():
        server_get( new uv_tcp_t ),
        client_req( new uv_stream_t )
{}

Server::~Server(){

    std::cout << "\033[1;5;31mProxy terminated\033[0m\n";
    uv_loop_close(uv_default_loop());
    delete client_req;
    delete server_get;
}

Server* Server::get_instance(){

    static Server server;
    return &server;
}

bool Server::init(const std::string& in_person_addr, int in_port) {

    uv_tcp_init(uv_default_loop(), &m_server);

    uv_ip4_addr((char *) in_person_addr.c_str(), in_port, &m_addr);

    uv_tcp_bind(&m_server, reinterpret_cast<const struct sockaddr *> (&m_addr), 0);

    error = uv_listen(reinterpret_cast<uv_stream_t *> (&m_server), 0, [](uv_stream_t *server, int status) {
        Server::get_instance()->on_new_connection(server, status);
    });
    if (error == 0) {
        std::cout << "\033[1;5;33m[INFO]\033[0m Proxy start: " << (char *) in_person_addr.c_str() << ":" << in_port
                  << '\n';
        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        return true;

    } else {
        std::cout << "\033[1;5;31m" << "Listen error: " << uv_strerror(error) << "\033[m\n";
        uv_close(reinterpret_cast<uv_handle_t *> (&m_server), nullptr);
        return false;

    }
}

void Server::alloc_server(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {

    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void Server::alloc_client_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {

    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void Server::alloc_server_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {

    buf->base = new char[suggested_size];
	buf->len = suggested_size;
}

void Server::on_client_read(uv_stream_t *client, ssize_t len, const uv_buf_t *buf) {

    auto connection_pos = open_sessions.find(client);
    Session get_client = connection_pos->second;

    if (connection_pos != open_sessions.end()) {
        if (len < 0) {
            std::cout << "\033[1;5;31m" << uv_err_name(len) << ": Read client error\n\033[0m";
            uv_close(reinterpret_cast<uv_handle_t *> (get_client.connection.get()), nullptr);
            delete[] buf->base;
        } else if (len > 0) {
            std::cout << "\033[1;5;34mProxy working client\n\033[0m";

            uv_buf_t buf_server = uv_buf_init(buf->base, len);
            error = uv_write(&m_server_wreq, reinterpret_cast<uv_stream_t *>(server_get), &buf_server, 1, nullptr);

            if (error == 0) {
                std::cout << "\033[1;5;34mWrite to server succeed!\n\033[0m";
            } else {
                std::cout << "\033[1;5;31mWrite to server error: " << uv_strerror(error) << "\033[0m\n";
            }

        }
    }
}

void Server::on_server_read(uv_stream_t *server, ssize_t len, const uv_buf_t *buf) {

    auto connection_pos = open_socket.find(server);
    Session get_server= connection_pos->second;

    if (connection_pos != open_socket.end()) {
        if (len < 0) {
            if (len == UV_EOF) {
                std::cout << "\033[1;5;31m" << "Client error: " << uv_err_name(len) << "\033[0m\n";
                uv_close(reinterpret_cast<uv_handle_t *>(get_server.connection.get()), nullptr);
                delete[] buf->base;
            } else {
                std::cout << "\033[1;5;31m" << uv_err_name(len) << ": Read server error\n\033[0m";
                uv_close(reinterpret_cast<uv_handle_t *>(get_server.connection.get()), nullptr);
                delete[] buf->base;
            }
        } else if (len > 0) {
            std::cout << "\033[1;5;34mProxy working server\n\033[0m";

            uv_buf_t buf_server = uv_buf_init(buf->base, len);
            error = uv_write(&m_client_wreq, client_req, &buf_server, 1, nullptr);

            if (error == 0) {
                std::cout << "\033[1;5;34mWrite to client succeed!\n\033[0m";
            } else {
                std::cout << "\033[1;5;31mWrite to client error: " << uv_strerror(error) << "\033[0m\n";
            }

        }
    }
}

void Server::on_server_conn(uv_connect_t *req, int status) {

    if (status < 0) {
        std::cout << "\033[1;5;31m" << "New server connecting error: " << uv_strerror(status) << "\033[0m\n";
    }

    std::cout << "\n\033[1;7;34mPROXY.\n\033[7;0m";

    auto connection_pos = open_socket.find(reinterpret_cast<uv_stream_t *>(req->handle));

    Session get;

    get = connection_pos->second;

    server_get = get.connection.get();

    if (connection_pos != open_socket.end()) {

        error = uv_read_start(client_req, [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                                  Server::get_instance()->alloc_client_buffer(stream, size, buf);
                              },
                              [](uv_stream_t *client, ssize_t len, const uv_buf_t *buf) {
                                  Server::get_instance()->on_client_read(client, len, buf);
                              });

        if (error == 0) {
            std::cout << "\033[1;5;34mRead to client start!\n\033[0m";
        } else {
            std::cout << "\033[1;5;31mRead to client error: " << uv_strerror(error) << "\033[0m\n";
        }

        error = uv_read_start(reinterpret_cast<uv_stream_t*>(req->handle), [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                                  Server::get_instance()->alloc_server_buffer(stream, size, buf);
                              },
                              [](uv_stream_t *server, ssize_t len, const uv_buf_t *buf) {
                                  Server::get_instance()->on_server_read(server, len, buf);
                              });

        if (error == 0) {
            std::cout << "\033[1;5;34mRead to server start!\n\033[0m";
        } else {
            std::cout << "\033[1;5;31mRead to server: " << uv_strerror(error) << "\033[0m\n";
        }
    }
}

void Server::proxy_start() {

    sock_session.connection = std::make_shared<uv_tcp_t>();

    uv_tcp_init(uv_default_loop(), sock_session.connection.get());

    uv_ip4_addr((char *) ip_req.c_str(), port, &req_addr);
    error = uv_tcp_connect(&m_server_req, sock_session.connection.get(), reinterpret_cast<const struct sockaddr *> (&req_addr),
                           [](uv_connect_t *req, int status) {
                               Server::get_instance()->on_server_conn(req, status);
                           });

    if (error == 0) {
        std::cout << "\033[1;5;34mConnecting to: " << ip_req << ':' << port << "\033[0m\n";
        auto *key = reinterpret_cast<uv_stream_t*> (sock_session.connection.get());
        open_socket.insert({key, sock_session});
    } else {
        std::cout << "\033[1;5;31mConnecting to ip error: " << uv_strerror(error) << "\033[0m\n";
        uv_close(reinterpret_cast<uv_handle_t*> (sock_session.connection.get()), nullptr);
        after_auth_answer[1] = 0x03;
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
        port = (256 * s) + static_cast<int>((static_cast<unsigned char>(msg[9])));
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
        sleep(1);
		std::cout << "\033[1;5;34mIP: " << ip_req << "\033[0m\n";
		std::cout << "\033[1;5;34mPORT: " << port << "\033[0m\n";

		return true;
	} else{
	    after_auth_answer[1] = 0x07;
	    return false;
	}
}

void Server::write_after_auth(const std::string& message_second, uv_stream_t *client) {

    int len = static_cast<int> (message_second.length());

    char buffer[len];

    uv_buf_t buf = uv_buf_init(buffer, len);

    buf.len = len;
    buf.base = (char *) message_second.c_str();

    int buf_count = 1;

    if (after_auth_answer[1] == 0x00) {
        client_req = client;
        Server::proxy_start();
        ip_req.clear();
    }

    error = uv_write(&m_write_req, client, &buf, buf_count, nullptr);

    if (error == 0) {
        std::cout << "\033[1;5;34mSend to client succeed!\033[0m\n";
    } else {
        std::cout << "\033[1;5;31mSend to client error: " << uv_strerror(error) << "\033[0m\n";
    }
}

void Server::write(const std::string& message_one, uv_stream_t *client) {

    int len = static_cast<int> (message_one.length());

    char buffer[len];

    uv_buf_t buf = uv_buf_init(buffer, len);

    buf.len = len;
    buf.base = (char *) message_one.c_str();

    int buf_count = 1;

    error = uv_write(&m_write_req, client, &buf, buf_count, nullptr);

    if (error == 0) {
        std::cout << "\033[1;5;34mSend to client succeed!\n\033[0m";
    } else {
        std::cout << "\033[1;5;31mSend to client error: " << uv_strerror(error) << "\033[0m\n";
    }

    error = uv_read_start(client, [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                              Server::get_instance()->alloc_server(stream, size, buf);
                          },
                          [](uv_stream_t *stream, ssize_t i, const uv_buf_t *buf) {
                              if (i < 0)
                                  std::cout << "\033[1;5;31m" << uv_err_name(i) << ": Read server error\n\033[0m";
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
            delete[] buf->base;
        } else if (i > 0) {
            uv_read_stop(client);

            std::string str(buf->base, i);

            if (Server::s5_parse_req(str)) {

                after_auth_answer[1] = 0x00;
                Server::write_after_auth(after_auth_answer, reinterpret_cast<uv_stream_t *>(get_client.connection.get()));
                delete[] buf->base;
            } else {
                std::cout << "\033[1;5;31mUnknown message\n\033[0m";
                after_auth_answer[1] = 0x08;
                Server::write_after_auth(after_auth_answer, reinterpret_cast<uv_stream_t *>(get_client.connection.get()));
                delete[] buf->base;
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
            delete[] buf->base;
        } else if (i > 0) {

            uv_read_stop(client);

            std::string str(buf->base, i);

            if (Server::s5_parse_auth(str)) {

                Server::write(auth_answer, reinterpret_cast<uv_stream_t *>(get_client.connection.get()));
            } else {
                std::cout << "\033[1;5;31mUnknown protocol\n\033[0m";
                delete[] buf->base;
            }
        }
    }
}

void Server::on_new_connection(uv_stream_t *server, int status) {

    if (status < 0) {
        std::cout << "\033[1;5;31mNew connection error: \033[0m" << uv_strerror(status) << '\n';
    }

    new_session.connection = std::make_shared<uv_tcp_t>();

    uv_tcp_init(uv_default_loop(), new_session.connection.get());

    if (uv_accept(server, reinterpret_cast<uv_stream_t *>(new_session.connection.get())) == 0){

        std::cout << "\033[1;5;33m[INFO] \033[1;5;39mNew connection.\n\033[0m";
        uv_read_start(reinterpret_cast<uv_stream_t *>(new_session.connection.get()),
                      [](uv_handle_t *stream, size_t size, uv_buf_t *buf) {
                          Server::get_instance()->alloc_server(stream, size, buf);
                      },
                      [](uv_stream_t *stream, ssize_t i, const uv_buf_t *buf) {
                          if (i < 0) std::cout << "\033[1;5;31m" << uv_err_name(i) << ": FIRST_SERVER_ERROR\n\033[0m";
                          Server::get_instance()->on_msg_recv(stream, i, buf);
                      });

        auto *key = reinterpret_cast<uv_stream_t *>(new_session.connection.get());
        open_sessions.insert({key, new_session});
    } else {
        uv_close(reinterpret_cast<uv_handle_t *>(new_session.connection.get()), nullptr);
    }
}
