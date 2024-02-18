#include "sum_reciever.hpp"
#include <asm-generic/socket.h>
#include <cerrno>
#include <ios>
#include <iostream>
#include <istream>
#include <ext/stdio_filebuf.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

sum_reciever::sum_reciever(const char* host, const char* port) {
    addrinfo hints{
        .ai_flags = 0,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
    };
    int err;
    if ((err = getaddrinfo(host, port, &hints, &addr_list)) != 0) {
        throw std::system_error(err, std::generic_category(), gai_strerror(err));
    }
}

std::istream sum_reciever::connect() {
    addrinfo *ai;
    for (ai = addr_list; ai != NULL; ai = ai->ai_next) {
        sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sockfd == -1) {
            continue;
        }
        linger linger{1, 0};
        setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
        if (::connect(sockfd, ai->ai_addr, ai->ai_addrlen) != -1) {
            break;
        }

        close(sockfd);
    }
    if (ai == NULL) {
        throw std::system_error(errno, std::generic_category());
    }

    fdbuf = __gnu_cxx::stdio_filebuf<char>(sockfd, std::ios::in);

    return std::istream(&fdbuf);
}

int sum_reciever::get_fd() const {
    return sockfd;
}

sum_reciever::~sum_reciever() {
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    freeaddrinfo(addr_list);
    std::cerr << "Reciever destructor\n";
}
