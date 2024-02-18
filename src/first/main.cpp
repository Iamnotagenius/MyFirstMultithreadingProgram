#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <mutex>
#include <numeric>
#include <ostream>
#include <string>
#include <system_error>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef DEBUG
#include "../debug/log.hpp"
#endif
#include "sum_sender.hpp"


void reader(std::mutex& new_data_lock, std::mutex& data_read_lock, std::string& buffer);
void writer(std::mutex& new_data_lock, std::mutex& data_read_lock, std::string& buffer, sum_sender& con);
std::string replace_even(const std::string& digits);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " PORT\n";
        return 1;
    }
    try {
        std::string buffer;
        std::mutex new_data;
        std::mutex data_handled;

        sum_sender con(argv[1]);

        buffer.reserve(128);
        new_data.lock();

        std::thread t1(reader, std::ref(new_data), std::ref(data_handled), std::ref(buffer));
        std::thread t2(writer, std::ref(new_data), std::ref(data_handled), std::ref(buffer), std::ref(con));
        std::thread conn_thread([&]() { con.handle_connections(); });

        t1.join();
        t2.join();
        conn_thread.detach();
    } catch (const std::system_error& e) {
        std::cerr << "Caught exception: " << e.what() << std::endl;
    }
}

void reader(std::mutex& new_data_lock, std::mutex& data_read_lock, std::string& buffer) {
    std::string input;
    while (!std::cin.eof()) {
        std::cout << "$ ";
        std::cin >> input;
        if (std::cin.eof()) {
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
        data_read_lock.lock();
        buffer = input;
        new_data_lock.unlock();
    }
    new_data_lock.unlock();
#ifdef DEBUG
            debug::debug_log("Reading thread") << "Terminating...\n";
#endif
}

void writer(std::mutex& new_data_lock, std::mutex& data_read_lock, std::string& buffer, sum_sender& con) {
    std::string input;
    while (!std::cin.eof()) {
#ifdef DEBUG
        debug::debug_log("Writing thread") << "Waiting for data...\n";
#endif
        new_data_lock.lock();
        input = buffer;
        buffer.clear();
        data_read_lock.unlock();

        if (std::cin.eof()) {
            break;
        }

        std::cout << input << std::endl;

        int sum = std::accumulate(input.cbegin(), input.cend(), 0, [](int lhs, char rhs) {
            if (isdigit(rhs)) {
                return lhs + (rhs - '0');
            }
            return lhs;
        });
        con.send_sum(sum);
    }
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
