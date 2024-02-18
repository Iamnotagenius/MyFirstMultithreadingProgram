#ifndef __SUM_RECIEVER_HPP
#define __SUM_RECIEVER_HPP

#include <istream>
#include <netdb.h>
#include <ext/stdio_filebuf.h>
class sum_reciever {
    private:
        addrinfo *addr_list;
        int sockfd;
        __gnu_cxx::stdio_filebuf<char> fdbuf;

    public:
        sum_reciever(const char* host, const char *port);
        std::istream connect();
        ~sum_reciever();
};
#endif
