#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <istream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <netdb.h>

#include "../debug/log.hpp"
#include "sum_reciever.hpp"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " HOST PORT\n";
        return 1;
    }
    sum_reciever rec(argv[1], argv[2]);
    while (true) {
#ifdef DEBUG
        debug::debug_log("Reciever") << "Trying to connect to sender at " << argv[1] << ':' << argv[2] << "...\n";
#endif
        try {
            std::istream con_stream = rec.connect();
#ifdef DEBUG
        debug::debug_log("Reciever") << "Connection established\n";
#endif
            while (!con_stream.eof()) {
                int sum;
                con_stream >> sum;
                if (sum > 9 && sum % 32 == 0) {
                    std::cout << "Got " << sum << "\n"; 
                }
                else {
                    std::cout << "Error: invalid sum\n";
                }
            }
        } catch (const std::system_error& e) {
            std::cerr << "Failed to connect: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    
    }
    
}
