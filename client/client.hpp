#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/context/all.hpp>

using boost::asio::ip::tcp;

enum {
    max_length = 1024
};

class client {
public:
    static void tcpClient(int num, char *info[]);

    static void boostContextTest();
};
