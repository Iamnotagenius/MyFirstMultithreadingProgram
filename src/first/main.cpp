#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <numeric>
#include <ostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef DEBUG
#include "../debug/log.hpp"
#endif
#include "connection.hpp"


void thread1(std::mutex& new_data_lock, std::mutex& data_read_lock, std::string& buffer);
void thread2(std::mutex& new_data_lock, std::mutex& data_read_lock, std::string& buffer);
std::string replace_even(const std::string& digits);

int main() {
    std::string buffer;
    std::mutex new_data;
    std::mutex data_handled;

    buffer.reserve(128);
    new_data.lock();

    std::thread t1(thread1, std::ref(new_data), std::ref(data_handled), std::ref(buffer));
    std::thread t2(thread2, std::ref(new_data), std::ref(data_handled), std::ref(buffer));

    t1.join();
    t2.join();
}

void thread1(std::mutex& new_data_lock, std::mutex& data_read_lock, std::string& buffer) {
    std::string input;
    while (!std::cin.eof()) {
        std::cout << "$ ";
        std::cin >> input;
        if (input.length() > 64 || std::any_of(input.cbegin(), input.cend(), std::not_fn(isdigit))) {
            std::cerr << "Message is invalid" << std::endl;
            continue;
        }
        std::sort(input.begin(), input.end(), std::greater());
        input = replace_even(input);
#ifdef DEBUG
        debug::debug_log("Reading thread") << "Waiting for data being read...\n";
#endif
        data_read_lock.lock();
        buffer = input;
        new_data_lock.unlock();
    }
}

void thread2(std::mutex& new_data_lock, std::mutex& data_read_lock, std::string& buffer) {
    std::string input;
    try {
        connection con(6666);
        std::thread conn_thread([&]() { con.handle_connections(); });
        while (!std::cin.eof()) {
#ifdef DEBUG
            debug::debug_log("Writing thread") << "Waiting for data...\n";
#endif
            new_data_lock.lock();
            input = buffer;
            buffer.clear();
            data_read_lock.unlock();

            std::cout << input << std::endl;

            int sum = std::accumulate(input.cbegin(), input.cend(), 0, [](int lhs, char rhs) {
                if (isdigit(rhs)) {
                    return lhs + (rhs - '0');
                }
                return lhs;
            });
            con.send_sum(sum);
        }
        conn_thread.join();
    } catch (const connection_exception& e) {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        exit(1);
    }
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
