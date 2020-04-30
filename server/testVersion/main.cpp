#include <iostream>
#include "server.hpp"

int main(int argc, char* argv[]) {

    if(argc < 2){
        std::cerr << "Insert a port\n";
    }

    Server *server = Server::get_instance();
    if(server->init("127.0.0.1", atoi(argv[1]))){
        std::cout << "Proxy init\n";
    }

    return 0;
}
