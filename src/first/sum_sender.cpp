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
#include <ostream>
#include <queue>
#include <set>
#include <string>
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

sum_sender::sum_sender(int port, int max_connections) 
            : sockfd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)),
                port(port),
                queue_mutex(),
                queue_condition(),
                to_send(),
                fds() {
    if (sockfd < 0) {
        throw connection_exception(0);
    }
    sockaddr_in addr;
    explicit_bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (sockaddr* )&addr, sizeof(addr)) < 0) {
        throw connection_exception(1);
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
        std::string str = std::to_string(sum) + '\n';

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
                    send(pfd.fd, str.data(), str.size(), 0);
                    sent = true;
                }
                pfd.fd = -1;
            }

            polling.erase(std::remove_if(polling.begin(), polling.end(), [](pollfd pfd){ return pfd.fd == -1; }), polling.end());
        }
        if (sent) {
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

static const char* messages[] = {"Failed to acquire socket", "Failed to bind socket"};
const char * connection_exception::what() const noexcept {
    return messages[error_code];
};
