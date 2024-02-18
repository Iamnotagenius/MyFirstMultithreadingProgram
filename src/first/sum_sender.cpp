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

        std::vector<pollfd> polling;
        polling.reserve(fds.size() + 1);
        std::transform(fds.cbegin(), fds.cend(), std::back_inserter(polling), [](int fd) {
            return pollfd{fd, POLLOUT, 0};
        });
        polling.push_back(pollfd{sockfd, POLLIN, 0});
        bool sent = false;
        while (!polling.empty()) {
#ifdef DEBUG
            debug::debug_log("Sender thread") << "polling " << polling.size() << " sockets\n";
#endif
            int ready = poll(polling.data(), polling.size(), -1);
#ifdef DEBUG
            debug::debug_log("Sender thread") << ready << " sockets are ready\n";
#endif
            if (polling.back().revents == POLLIN) {
#ifdef DEBUG
                debug::debug_log("Sender thread") << "Socket is ready to accept connections\n";
#endif
                std::vector<int> new_fds = accept_new_connections();
                polling.pop_back();
                polling.reserve(polling.size() + new_fds.size() + 1);
                std::transform(new_fds.cbegin(), new_fds.cend(), std::back_inserter(polling), [](int fd){
                    return pollfd{fd, POLLOUT, 0};
                });
                polling.push_back(pollfd{sockfd, POLLIN, 0});
                continue;
            }
            else if (polling.back().revents == 0) {
                polling.pop_back();
            }
            else {
                perror("Error on socket");
                return;
            }
            if (ready == -1) {
                perror("Error on poll()");
                break;
            }
            for (pollfd& pfd : polling) {
                if (pfd.revents == 0) {
                    return;
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

std::vector<int> sum_sender::accept_new_connections() {
    std::vector<int> new_fds;
    int new_fd;
    errno = 0;
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
        new_fds.push_back(new_fd);
    } while (new_fd != -1);

    return new_fds;
}

void sum_sender::send_sum(int sum) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        to_send.push(sum);
    }
    queue_condition.notify_one();
}

void sum_sender::notify_eof() {
    queue_condition.notify_one();
}

sum_sender::~sum_sender() {
#ifdef DEBUG
    debug::debug_log("Sender") << "Terminating connections...\n";
#endif
    queue_condition.notify_one();
    for (int fd : fds) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

