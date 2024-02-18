#include <algorithm>
#include <asm-generic/socket.h>
#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <mutex>
#include <netdb.h>
#include <ostream>
#include <queue>
#include <set>
#include <string>
#include <system_error>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <errno.h>
#include <vector>
#include <fcntl.h>

#ifdef DEBUG
#include "../debug/log.hpp"
#endif
#include "sum_sender.hpp"

sum_sender::sum_sender(const char* port, int max_connections) 
            : queue_mutex(),
                queue_condition(),
                to_send(),
                fds() {
    addrinfo hints{
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0
    };
    addrinfo *addr_list;
    int err;
    if ((err = getaddrinfo(NULL, port, &hints, &addr_list)) != 0) {
        throw std::system_error(err, std::generic_category(), gai_strerror(err));
    }
        
    addrinfo *ai;
    for (ai = addr_list; ai != NULL; ai = ai->ai_next) {
        sockfd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
        if (sockfd == -1) {
            continue;
        }
        int optval = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;
        }

        close(sockfd);
    }
    if (ai == NULL) {
        throw std::system_error(errno, std::generic_category());
    }

    listen(sockfd, max_connections);
}

void sum_sender::handle_connections() {
    while (!std::cin.eof() || !to_send.empty()) {
#ifdef DEBUG
        debug::debug_log("Sender thread") << "Waiting for sum...\n";
#endif
        if (to_send.empty()) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_condition.wait(lock, [this]{ return !to_send.empty() || std::cin.eof(); });
        }
        if (to_send.empty()) {
            break;
        }
        int sum = to_send.front();
#ifdef DEBUG
        debug::debug_log("Sender thread") << "Got sum " << sum << "\n";
#endif

        accept_new_connections();

        std::vector<pollfd> polling;
        polling.reserve(fds.size());
        std::transform(fds.cbegin(), fds.cend(), std::back_inserter(polling), [](int fd) {
            return pollfd{fd, POLLOUT, 0};
        });
        bool sent = false;
        while (!polling.empty()) {
            int ready = poll(polling.data(), polling.size(), -1);
            if (ready == -1) {
                perror("Error on poll()");
                break;
            }
            for (pollfd& pfd : polling) {
                if (pfd.revents == 0) {
                    continue;
                }
                if (pfd.revents != POLLOUT) {
                    fds.erase(pfd.fd);
                    close(pfd.fd);
                }
                else {
                    std::string str = std::to_string(sum) + '\n';
                    send(pfd.fd, str.data(), str.size(), 0);
                    sent = true;
                }
                pfd.fd = -1;
            }

            polling.erase(std::remove_if(polling.begin(), polling.end(), [](pollfd pfd){ return pfd.fd == -1; }), polling.end());
        }
        if (sent) {
#ifdef DEBUG
            debug::debug_log("Sender thread") << "Sent " << sum << std::endl;
#endif
            to_send.pop();
        }
#ifdef DEBUG
        else {
            debug::debug_log("Sender thread") << "Failed to send, do not pop " << sum << std::endl;
        }
#endif
    }
#ifdef DEBUG
            debug::debug_log("Sender thread") << "Terminating...\n";
#endif
}

void sum_sender::accept_new_connections() {
    int new_fd;
    errno = 0;
    if (fds.empty()) {
        int flags = fcntl(sockfd, F_GETFL);
        fcntl(sockfd, F_SETFL, flags & (~O_NONBLOCK));
#ifdef DEBUG
        debug::debug_log("Sender thread") << "Waiting for new connections...\n";
#endif
        new_fd = accept(sockfd, NULL, NULL);
        if (new_fd < 0) {
            perror("Error on accept()");
            exit(1);
        }
        else {
#ifdef DEBUG
            debug::debug_log("Sender thread") << "New connection established " << new_fd << std::endl;
#endif
            fds.insert(new_fd);
        }
        fcntl(sockfd, F_SETFL, flags);
    }

    do {
        new_fd = accept(sockfd, NULL, NULL);
        if (new_fd < 0 && errno != EWOULDBLOCK) {
            perror("Error on accept()");
            exit(1);
        }
        if (errno == EWOULDBLOCK) {
            break;
        }
#ifdef DEBUG
        debug::debug_log("Sender thread") << "New connection established " << new_fd << std::endl;
#endif
        fds.insert(new_fd);
    } while (new_fd != -1);
}

void sum_sender::send_sum(int sum) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        to_send.push(sum);
    }
    queue_condition.notify_one();
}

sum_sender::~sum_sender() {
#ifdef DEBUG
    debug::debug_log("Sender") << "Terminating connections...\n";
#endif
    queue_condition.notify_one();
    for (int fd : fds) {
        close(fd);
    }
    close(sockfd);
}

