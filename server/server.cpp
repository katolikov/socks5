#include "server.hpp"

Server::Server():
          loop(uv_default_loop()),
          m_server_req(new uv_connect_t),
          m_server(new uv_tcp_t)
{
  read_buffer.resize(MAX_BUFF_SIZE);
}

Server::~Server()
{
    std::cout << "\033[1;5;31mProxy terminated\033[0m\n";
}

Server* Server::get_instance()
{
    static Server server;
    return &server;
}

msg_buffer* Server::get_read_buffer()
{
    return &read_buffer;
}

bool Server::init(const std::string& in_person_addr, int in_port) {

    error = uv_tcp_init(loop.get(), m_server.get());

    if(error != 0){
        std::cout << "\033[1;5;31m" << "Init error: " << uv_strerror(error) << "\033[m\n";
    }

    error = uv_ip4_addr((char *) in_person_addr.c_str(), in_port, &m_addr);

    if(error != 0){
        std::cout << "\033[1;5;31m" << "uv_ip4_addr error: " << uv_strerror(error) << "\033[m\n";
    }

    error = uv_tcp_bind(m_server.get(), reinterpret_cast<const struct sockaddr *> (&m_addr), 0);

    if(error != 0){
        std::cout << "\033[1;5;31m" << "Bind error: " << uv_strerror(error) << "\033[m\n";
    }

    error = uv_listen(reinterpret_cast<uv_stream_t *> (m_server.get()), 0, [](uv_stream_t *server, int status) {
        Server::get_instance()->on_new_connection(server, status);
    });

    if (error == 0) {
        std::cout << "\033[1;5;33m[INFO]\033[0m Proxy start: " << (char *) in_person_addr.c_str() << ":" << in_port
                  << '\n';
        uv_run(loop.get(), UV_RUN_DEFAULT);
        return true;

    } else {
        std::cout << "\033[1;5;31m" << "Listen error: " << uv_strerror(error) << "\033[m\n";
        return false;

    }
}

void Server::alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {

    Server* server = Server::get_instance();
    msg_buffer* read_v = server->get_read_buffer();
    *buf = uv_buf_init(read_v->data(), read_v->capacity());
}

void Server::on_client_read(uv_stream_t *client, ssize_t len, const uv_buf_t *buf) {

  auto connection_cl = open_sessions.find(client);
  Session get_client = connection_cl->second;
  auto connection_sr = server_socket.find(client);
  Session get_server = connection_sr->second;

  uv_read_stop(
         reinterpret_cast<uv_stream_t *>(get_client.connection.get()));

  if (connection_cl != open_sessions.end()) {
    if(len < 0){
        if (len == UV_EOF) {
            std::cout << "\033[1;5;31m" << "Client EOF: "
                      << uv_strerror(len) << "\033[0m\n";
            remove_proxy_client(reinterpret_cast<uv_stream_t *>(get_client.connection.get()));

        } else {
            std::cout << "\033[1;5;31m"
                      << uv_strerror(len) << ": Read client error\n\033[0m";
            remove_proxy_client(reinterpret_cast<uv_stream_t *>(get_client.connection.get()));
        }
    } else if (len > 0) {
            std::cout << "\033[1;5;34mProxy working client\n\033[0m";

            uv_buf_t buf_server = uv_buf_init(buf->base, len);
            uv_write_t * m_server_wreq = (uv_write_t *) malloc(sizeof(uv_write_t));

            error = uv_write(m_server_wreq,
              reinterpret_cast<uv_stream_t *> (get_server.connection.get()),
                &buf_server, 1, nullptr);

            if (error == 0) {
                std::cout << "\033[1;5;34mWrite to server succeed!\n\033[0m";
            } else {
                std::cout << "\033[1;5;31mWrite to server error: " << uv_strerror(error) << "\033[0m\n";
                remove_proxy(reinterpret_cast<uv_stream_t *> (get_server.connection.get()));
            }

        }
    }
}

void Server::on_server_read(uv_stream_t *server, ssize_t len, const uv_buf_t *buf) {

  auto connection_pos = open_socket.find(server);
  Session get_server= connection_pos->second;
  auto connection_cl = client_socket.find(server);

  uv_read_stop(
          reinterpret_cast<uv_stream_t *>(get_server.connection.get()));

  if (connection_pos != open_socket.end()) {
      if (len < 0) {
          if (len == UV_EOF) {
                std::cout << "\033[1;5;31m" << "Server EOF: "
                          << uv_strerror(len) << "\033[0m\n";
                remove_proxy(reinterpret_cast<uv_stream_t *> (get_server.connection.get()));
            } else {
                std::cout << "\033[1;5;31m"
                          << uv_strerror(len) << ": Read server error\n\033[0m";
                remove_proxy(reinterpret_cast<uv_stream_t *> (get_server.connection.get()));
              }
        } else if (len > 0) {
            std::cout << "\033[1;5;34mProxy working server\n\033[0m";

            uv_buf_t buf_server = uv_buf_init(buf->base, len);

            uv_write_t * m_client_wreq = msg.requests.get_new();

            error = uv_write(m_client_wreq, connection_cl->second,
                &buf_server, 1, nullptr);

            if (error == 0) {
                std::cout << "\033[1;5;34mWrite to client succeed!\n\033[0m";
            } else {
                std::cout << "\033[1;5;31mWrite to client error: " << uv_strerror(error) << "\033[0m\n";
                remove_proxy_client(connection_cl->second);
            }

        }
    }
}

void Server::on_server_conn(uv_connect_t *req, int status) {

    if (status < 0) {
        std::cout << "\033[1;5;31m"
                << "New server connecting error: "
                << uv_strerror(status) << "\033[0m\n";
    }

    std::cout << "\n\033[1;7;34mPROXY.\n\033[7;0m";

    auto connection_pos = open_socket.find(
                    reinterpret_cast<uv_stream_t*>(req->handle));

    auto connection_cl = client_socket.find(
                    reinterpret_cast<uv_stream_t*>(req->handle));

    Session get = connection_pos->second;

    if (connection_pos != open_socket.end()) {

        error = uv_read_start(connection_cl->second, alloc_buffer,
                              [](uv_stream_t *client, ssize_t len, const uv_buf_t *buf) {
                                  Server::get_instance()->on_client_read(client, len, buf);
                              });

        if (error == 0) {
            std::cout << "\033[1;5;34mRead to client start!\n\033[0m";
        } else {
            std::cout << "\033[1;5;31mRead to client error: " << uv_strerror(error) << "\033[0m\n";
            remove_proxy_client(connection_cl->second);
        }

        error = uv_read_start(reinterpret_cast<uv_stream_t*>(get.connection.get()), alloc_buffer,
                              [](uv_stream_t *server, ssize_t len, const uv_buf_t *buf) {
                                  Server::get_instance()->on_server_read(server, len, buf);
                              });

        if (error == 0) {
            std::cout << "\033[1;5;34mRead to server start!\n\033[0m";
        } else {
            std::cout << "\033[1;5;31mRead to server: " << uv_strerror(error) << "\033[0m\n";
            remove_proxy(reinterpret_cast<uv_stream_t *> (get.connection.get()));
        }
    }
}

void Server::proxy_start(uv_stream_t *client) {

    sock_session.connection = std::make_shared<uv_tcp_t>();
    sock_session.activity_timer = std::make_shared<uv_timer_t>();

    uv_timer_init(loop.get(), sock_session.activity_timer.get());

    error = uv_tcp_init(loop.get(), sock_session.connection.get());

    if (error == 0) {

        error = uv_ip4_addr((char *) ip_req.c_str(), port, &req_addr);

        if (error != 0) {
              std::cout << "\033[1;5;31mIP4 addr libuv error: "
                          << uv_strerror(error) << "\033[0m\n";
        }

        error = uv_tcp_connect(m_server_req.get(), sock_session.connection.get(),
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
              client_socket.insert({key, client});
              server_socket.insert({client, sock_session});

              active_timers_proxy.insert({sock_session.activity_timer.get(), key});

              uv_timer_start(sock_session.activity_timer.get(),
                             [] (uv_timer_t* handle)
                             {Server::get_instance()->on_prxoy_timeout(handle);
                             },TIME, 0);


          } else {
              std::cout << "\033[1;5;31mConnecting to ip error: "
                        << uv_strerror(error) << "\033[0m\n";
              after_auth_answer[1] = 0x03;
              remove_proxy(
                reinterpret_cast<uv_stream_t *>(sock_session.connection.get()));
      }
    } else {
        std::cout << "\033[1;5;31mConnecting to ip error: " << uv_strerror(error) << "\033[0m\n";
        after_auth_answer[1] = 0x03;
        remove_proxy(
          reinterpret_cast<uv_stream_t *>(sock_session.connection.get()));
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

    auto connection_pos = open_sessions.find(client);
    Session get_client = connection_pos->second;

    int len = static_cast<int> (message_second.length());

    char buffer[len];

    uv_buf_t buf = uv_buf_init(buffer, len);

    buf.len = len;
    buf.base = (char *) message_second.c_str();

    int buf_count = 1;

    if (after_auth_answer[1] == 0x00) {
        Server::proxy_start(
          reinterpret_cast<uv_stream_t *>(get_client.connection.get()));
        ip_req.clear();
    }
    uv_write_t * m_write_req= msg.requests.get_new();

    error = uv_write(m_write_req,
      reinterpret_cast<uv_stream_t *>(get_client.connection.get()),
      &buf, buf_count, nullptr);

    if (error == 0) {
        //std::cout << "\033[1;5;34mSend to client succeed!\033[0m\n";
    } else {
        std::cout << "\033[1;5;31mSend to client error: " << uv_strerror(error) << "\033[0m\n";
        remove_client(
          reinterpret_cast<uv_stream_t *>(get_client.connection.get()));
    }
}

void Server::write(const std::string& message_one, uv_stream_t *client) {

    auto connection_pos = open_sessions.find(client);
    Session get_client = connection_pos ->second;

    int len = static_cast<int> (message_one.length());

    char buffer[len];

    uv_buf_t buf = uv_buf_init(buffer, len);

    buf.len = len;
    buf.base = (char *) message_one.c_str();

    int buf_count = 1;

    uv_write_t * m_write_req= msg.requests.get_new();

    error = uv_write(m_write_req,
      reinterpret_cast<uv_stream_t *>(get_client.connection.get()),
      &buf, buf_count, nullptr);

    if (error == 0) {
        //std::cout << "\033[1;5;34mSend to client succeed!\n\033[0m";
    } else {
        std::cout << "\033[1;5;31mSend to client error: " << uv_strerror(error) << "\033[0m\n";
        remove_client(
          reinterpret_cast<uv_stream_t *>(get_client.connection.get()));
    }

    error = uv_read_start(reinterpret_cast<uv_stream_t *>(get_client.connection.get()),
                          alloc_buffer,
                          [](uv_stream_t *stream, ssize_t i, const uv_buf_t *buf) {
                              if (i < 0)
                                  std::cout << "\033[1;5;31m" << uv_err_name(i) << ": Read server error\n\033[0m";
                              Server::get_instance()->second_msg_recv(stream, i, buf);
                          });

    if (error == 0) {
        //std::cout << "\033[1;5;34mRead to server start!\n\033[0m";
    } else {
        std::cout << "\033[1;5;31mRead to server: " << uv_strerror(error) << "\033[0m\n";
        remove_client(
          reinterpret_cast<uv_stream_t *>(get_client.connection.get()));
    }
}

void Server::second_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf) {

    auto connection_pos = open_sessions.find(client);
    Session get_client = connection_pos ->second;

    if (connection_pos != open_sessions.end()) {
        bool reset_timer = true;
        if (i == UV_EOF) {
            reset_timer = false;
            std::cout << "\033[1;5;31mClient Disconnected\n\033[0m";
            remove_client(
              reinterpret_cast<uv_stream_t *>(get_client.connection.get()));

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
        } else if (reset_timer){
          uv_timer_start(connection_pos->second.activity_timer.get(),
                    [] (uv_timer_t* handle){
                        Server::get_instance()->on_client_timeout(handle);},
                    TIME,0);
        }
    }
}

void Server::on_msg_recv(uv_stream_t *client, ssize_t i, const uv_buf_t *buf) {

    auto connection_pos = open_sessions.find(client);
    Session get_client = connection_pos ->second;

    if (connection_pos != open_sessions.end()) {
        bool reset_timer = true;
        if (i == UV_EOF) {
            reset_timer = false;
            std::cout << "\033[1;5;31mClient Disconnected\n\033[0m";
            remove_client(
              reinterpret_cast<uv_stream_t *>(get_client.connection.get()));

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
        } else if (reset_timer){
          uv_timer_start(connection_pos->second.activity_timer.get(),
                    [] (uv_timer_t* handle){
                        Server::get_instance()->on_client_timeout(handle);},
                    TIME,0);
        }
    }
}

void Server::remove_client(uv_stream_t* client)
{
    auto connection_pos = open_sessions.find(client);
    if (connection_pos != open_sessions.end())
    {
        uv_timer_stop(connection_pos->second.activity_timer.get());
        active_timers.erase(connection_pos->second.activity_timer.get());
        uv_read_stop(client);

        uv_close((uv_handle_t*)connection_pos->second.connection.get(),
                 [] (uv_handle_t* handle){
                Server::get_instance()->on_connection_close_client(handle);});
    }
}

void Server::remove_proxy(uv_stream_t* client)
{
    auto connection_pos = open_sessions.find(client);
    if (connection_pos != open_sessions.end())
    {
        uv_timer_stop(connection_pos->second.activity_timer.get());
        active_timers.erase(connection_pos->second.activity_timer.get());
        uv_read_stop(client);

        uv_close((uv_handle_t*)connection_pos->second.connection.get(),
                 [] (uv_handle_t* handle){
                Server::get_instance()->on_connection_close_proxy(handle); });
    }
}

void Server::remove_proxy_client(uv_stream_t* client)
{
    auto connection_pos = open_sessions.find(client);
    if (connection_pos != open_sessions.end())
    {
        uv_timer_stop(connection_pos->second.activity_timer.get());
        active_timers.erase(connection_pos->second.activity_timer.get());
        uv_read_stop(client);

        uv_close((uv_handle_t*)connection_pos->second.connection.get(),
                 [] (uv_handle_t* handle){
                Server::get_instance()->on_connection_close_proxy_client(handle);});
    }
}

void Server::on_connection_close_client(uv_handle_t* handle)
{
    open_sessions.erase((uv_stream_t*)handle);
}

void Server::on_connection_close_proxy(uv_handle_t* handle)
{
    open_sessions.erase((uv_stream_t*)handle);
}

void Server::on_connection_close_proxy_client(uv_handle_t* handle)
{
    open_sessions.erase((uv_stream_t*)handle);
}

void Server::on_client_timeout(uv_timer_t* handle)
{
    auto timer_pos = active_timers.find(handle);
    if (timer_pos != active_timers.end())
    {
        auto connection_pos = open_sessions.find(timer_pos->second);
        if (connection_pos != open_sessions.end())
        {
            Session* s = &(connection_pos->second);
            remove_client((uv_stream_t*)s->connection.get());
        }
    }
}

void Server::on_prxoy_timeout(uv_timer_t* handle)
{
    auto timer_pos = active_timers.find(handle);
    if (timer_pos != active_timers.end())
    {
        auto connection_pos = open_sessions.find(timer_pos->second);
        if (connection_pos != open_sessions.end())
        {
            Session* s = &(connection_pos->second);
            remove_proxy((uv_stream_t*)s->connection.get());
        }
    }
}

void Server::on_prxoy_client_timeout(uv_timer_t* handle)
{
    auto timer_pos = active_timers.find(handle);
    if (timer_pos != active_timers.end())
    {
        auto connection_pos = open_sessions.find(timer_pos->second);
        if (connection_pos != open_sessions.end())
        {
            Session* s = &(connection_pos->second);
            remove_proxy_client((uv_stream_t*)s->connection.get());
        }
    }
}

void Server::on_new_connection(uv_stream_t *server, int status) {

    if (status == 0) {

        new_session.connection = std::make_shared<uv_tcp_t>();
        new_session.activity_timer = std::make_shared<uv_timer_t>();

        error = uv_tcp_init(loop.get(), new_session.connection.get());

        if(error != 0){
          std::cout << "\033[1;5;31m"
                << uv_strerror(error) << ": INIT ERROR\n\033[0m";
        }

        error = uv_timer_init(loop.get(), new_session.activity_timer.get());

        if(error != 0){
          std::cout << "\033[1;5;31m"
                << uv_strerror(error) << ": TIMER ERROR\n\033[0m";
        }

        if (uv_accept(server, reinterpret_cast<uv_stream_t *>(new_session.connection.get())) == 0){

            std::cout << "\033[1;5;33m[INFO] \033[1;5;39mNew connection.\n\033[0m";
            error = uv_read_start(reinterpret_cast<uv_stream_t *>(new_session.connection.get()),
                          alloc_buffer,
                          [](uv_stream_t *stream, ssize_t i, const uv_buf_t *buf) {
                              if (i < 0) std::cout << "\033[1;5;31m" << uv_err_name(i) << ": FIRST_SERVER_ERROR\n\033[0m";
                              Server::get_instance()->on_msg_recv(stream, i, buf);
                          });

            if(error == 0){
              auto *key = reinterpret_cast<uv_stream_t *>(new_session.connection.get());
              open_sessions.insert({key, new_session});
              active_timers.insert({new_session.activity_timer.get(), key});

              uv_timer_start(new_session.activity_timer.get(),
                             [] (uv_timer_t* handle){
                                 Server::get_instance()->on_client_timeout(handle);},
                             TIME,0);
            } else {
              std::cout << "\033[1;5;31m"
                    << uv_strerror(error) << ": READ ERROR\n\033[0m";
              remove_client(
                reinterpret_cast<uv_stream_t *>(new_session.connection.get()));
            }

        } else {
          std::cout << "\033[1;5;31m"
                << uv_strerror(error) << ": ACCEPT ERROR\n\033[0m";
        }
   }
}
