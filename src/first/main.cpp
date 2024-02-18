#include <algorithm>
#include <array>
#include <atomic>
#include <bits/types/sigset_t.h>
#include <cctype>
#include <condition_variable>
#include <csignal>
#include <functional>
#include <iostream>
#include <mutex>
#include <numeric>
#include <ostream>
#include <pthread.h>
#include <string>
#include <sys/poll.h>
#include <sys/signalfd.h>
#include <system_error>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#ifdef DEBUG
#include "../debug/log.hpp"
#endif
#include "sum_sender.hpp"


void reader(std::mutex& buffer_mutex, std::condition_variable& condition, std::string& buffer, const std::atomic_bool& should_stop, int sigfd);
void writer(std::mutex& buffer_mutex, std::condition_variable& condition, std::string& buffer, sum_sender& sender, const std::atomic_bool& should_stop);
std::string replace_even(const std::string& digits);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " PORT\n";
        return 1;
    }
    try {
        std::string buffer;
        std::mutex buffer_mutex;
        std::condition_variable condition;
        std::atomic_bool should_stop = false;
        sum_sender sender(argv[1]);

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &set, NULL);
        int sigfd = signalfd(-1, &set, 0);

        buffer.reserve(128);

        std::thread t1(reader, std::ref(buffer_mutex), std::ref(condition), std::ref(buffer), std::cref(should_stop), sigfd);
        std::thread t2(writer, std::ref(buffer_mutex), std::ref(condition), std::ref(buffer), std::ref(sender), std::cref(should_stop));
        std::thread conn_thread([&]() { sender.handle_connections(sigfd, should_stop); });

        std::thread sig_thread([&]() {
            int sig;
            sigwait(&set, &sig);
#ifdef DEBUG
            debug::debug_log("Signal handling thread") << "Signal caught: " << sig << '\n';
#endif
            should_stop = true;
            condition.notify_all();
            sender.notify_eof();
        });
        sig_thread.detach();

        t1.join();
        t2.join();
        sender.notify_eof();
        conn_thread.join();
    } catch (const std::system_error& e) {
        std::cerr << "System error: " << e.what() << std::endl;
    }
}

void reader(std::mutex& buffer_mutex, std::condition_variable& condition, std::string& buffer, const std::atomic_bool& should_stop, int sigfd) {
    std::string input;
    std::array<pollfd, 2> polling{
        pollfd{sigfd, POLLIN, 0},
        pollfd{STDIN_FILENO, POLLIN, 0}
    };
    while (!std::cin.eof() && !should_stop) {
        poll(polling.data(), polling.size(), -1);
        if (polling[1].revents == POLLIN) {
            std::cin >> input;
        }
        else {
            break;
        }

        if (input.length() > 64 || std::any_of(input.cbegin(), input.cend(), std::not_fn(isdigit))) {
            std::cerr << "Message is invalid" << std::endl;
            continue;
        }
        std::sort(input.begin(), input.end(), std::greater());
        input = replace_even(input);
#ifdef DEBUG
        debug::debug_log("Reading thread") << "Waiting for data being read...\n";
#endif
        {
            std::unique_lock<std::mutex> lock(buffer_mutex);
            condition.wait(lock, [&](){ return buffer.empty() || std::cin.eof() || should_stop; });
            if (std::cin.eof() || should_stop) {
                break;
            }
            buffer = input;
        }
        condition.notify_one();
    }
    condition.notify_one();
#ifdef DEBUG
            debug::debug_log("Reading thread") << "Terminating...\n";
#endif
}

void writer(std::mutex& buffer_mutex, std::condition_variable& condition, std::string& buffer, sum_sender& sender, const std::atomic_bool& should_stop) {
    std::string input;
    while (!std::cin.eof()) {
        {
            std::unique_lock<std::mutex> lock(buffer_mutex);
#ifdef DEBUG
            debug::debug_log("Writing thread") << "Waiting for data...\n";
#endif
            condition.wait(lock, [&](){ return !buffer.empty() || std::cin.eof() || should_stop; });
            if (std::cin.eof() || should_stop) {
                break;
            }
            input = buffer;
            buffer.clear();
        }
        condition.notify_one();

        if (std::cin.eof()) {
            break;
        }

        std::cout << input << '\n';

        int sum = std::accumulate(input.cbegin(), input.cend(), 0, [](int lhs, char rhs) {
            if (isdigit(rhs)) {
                return lhs + (rhs - '0');
            }
            return lhs;
        });
        sender.send_sum(sum);
    }
    condition.notify_one();
#ifdef DEBUG
    debug::debug_log("Writing thread") << "Terminating...\n";
#endif
}

std::string replace_even(const std::string& digits) {
    std::string result;
    result.reserve(digits.length());
    for (auto c : digits) {
        if ((c - '0') % 2 == 0) {
            result.append("KB");
        }
        else {
            result.push_back(c);
        }
    }
    return result;
}
