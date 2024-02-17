#ifndef __SUM_RECIEVER_HPP
#define __SUM_RECIEVER_HPP

#include <istream>
#include <netinet/in.h>
#include <ext/stdio_filebuf.h>
class sum_reciever {
    private:
        int sockfd;
        sockaddr_in addr;
        __gnu_cxx::stdio_filebuf<char> fdbuf;

    public:
        sum_reciever(const sockaddr_in& addr);
        std::istream connect();
        ~sum_reciever();
};
#endif
