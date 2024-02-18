#include <array>
#include <bits/types/sigset_t.h>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <istream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ostream>
#include <ratio>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <netdb.h>
#include <sys/signalfd.h>

#ifdef DEBUG
#include "../debug/log.hpp"
#endif
#include "sum_reciever.hpp"

static volatile std::sig_atomic_t should_stop = 0;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " HOST PORT\n";
        return 1;
    }

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigprocmask(SIG_BLOCK, &set, NULL);
    int sigfd = signalfd(-1, &set, 0);
    std::array<pollfd, 2> polling{pollfd{sigfd, POLLIN, 0}};
    sum_reciever rec(argv[1], argv[2]);
    while (!should_stop) {
#ifdef DEBUG
        debug::debug_log("Reciever") << "Trying to connect to sender at " << argv[1] << ':' << argv[2] << "...\n";
#endif
        try {
            std::istream con_stream = rec.connect();
            polling[1] = pollfd{rec.get_fd(), POLLIN, 0};
#ifdef DEBUG
        debug::debug_log("Reciever") << "Connection established\n";
#endif
            while (!con_stream.eof() && !should_stop) {
                int sum;
                int ready = poll(polling.data(), polling.size(), -1);
                if (ready < 0) {
                    perror("poll()");
                    break;
                }
                if (polling[0].revents == POLLIN) {
#ifdef DEBUG
                    debug::debug_log("Reciever") << "Terminating gracefully...\n";
#endif
                    return 0;
                }
                else if (polling[1].revents == POLLIN) {
                    con_stream >> sum;
                }

                if (con_stream.fail()) {
#ifdef DEBUG
                    perror("Stream error");
#endif
                    break;
                }
                if (sum > 9 && sum % 32 == 0) {
                    std::cout << "Got " << sum << "\n"; 
                }
                else {
                    std::cout << "Error: invalid sum " << sum << "\n";
                }
            }
        } catch (const std::system_error& e) {
            std::cerr << "Failed to connect: " << e.what() << std::endl;
            poll(polling.data(), 1, std::chrono::milliseconds(std::chrono::seconds(1)).count());
            if (polling[0].revents == POLLIN) {
#ifdef DEBUG
                debug::debug_log("Reciever") << "Terminating gracefully...\n";
#endif
                return 0;
            }
        }
    }
}
