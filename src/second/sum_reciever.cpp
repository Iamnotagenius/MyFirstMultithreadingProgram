#include "sum_reciever.hpp"
#include <cerrno>
#include <ios>
#include <iostream>
#include <istream>
#include <ext/stdio_filebuf.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

sum_reciever::sum_reciever(const sockaddr_in& addr) :
        sockfd(socket(AF_INET, SOCK_STREAM, 0)), addr(addr), fdbuf(sockfd, std::ios::in) {
    if (sockfd < 0) {
        throw std::system_error(errno, std::generic_category());
    }
}

std::istream sum_reciever::connect() {
    int connfd = ::connect(sockfd, (sockaddr* )&addr, sizeof(addr));
    if (connfd < 0) {
        close(sockfd);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        throw std::system_error(errno, std::generic_category());
    }
    return std::istream(&fdbuf);
}

sum_reciever::~sum_reciever() {
    close(sockfd);
}
