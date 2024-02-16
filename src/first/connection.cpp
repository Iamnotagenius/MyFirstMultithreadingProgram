#include <algorithm>
#include <asm-generic/socket.h>
#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
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

#include "connection.hpp"

connection::connection(int port, int max_connections) 
            : sockfd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)),
                port(port),
                queue_mutex(),
                queue_condition(),
                to_send(),
                fds() {
    if (sockfd < 0) {
        throw connection_exception(0);
    }
    /* fcntl(sockfd, F_SETOWN, getpid()); */
    /* fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_ASYNC); */
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

void connection::handle_connections() {
    while (true) {
        std::cerr << "Waiting for sum...\n";
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_condition.wait(lock, [this]{ return !to_send.empty(); });
        }
        int sum = to_send.front();
        to_send.pop();
        std::cerr << "Got sum " << sum << "\n";
        std::string str = std::to_string(sum);

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
                    std::cerr << "no events on " << pfd.fd << std::endl;
                    continue;
                }
                if (pfd.revents != POLLOUT) {
                    std::cerr << "Unexpected event on " << pfd.fd << ": closing connection" << std::endl;
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
        if (!sent) {
            std::cerr << "Failed to send, repushing " << sum << std::endl;
            to_send.push(sum);
        }
    }
}

void connection::accept_new_connections() {
    int new_fd;
    errno = 0;
    if (fds.empty()) {
        int flags = fcntl(sockfd, F_GETFL);
        fcntl(sockfd, F_SETFL, flags & (~O_NONBLOCK));
        std::cerr << "Waiting for new connections...\n";
        new_fd = accept(sockfd, NULL, NULL);
        if (new_fd < 0) {
            perror("Error on accept()");
            exit(1);
        }
        else {
            std::cerr << "New connection established " << new_fd << std::endl;
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
        std::cerr << "New connection established " << new_fd << std::endl;
        fds.insert(new_fd);
    } while (new_fd != -1);
}

void connection::send_sum(int sum) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        to_send.push(sum);
    }
    queue_condition.notify_all();
}

connection::~connection() {
    for (int fd : fds) {
        close(fd);
    }
    close(sockfd);
}

static const char* messages[] = {"Failed to acquire socket", "Failed to bind socket"};
const char * connection_exception::what() const noexcept {
    return messages[error_code];
};

