#include <chrono>
#include <iostream>
#include <istream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <thread>

#include "../debug/log.hpp"
#include "sum_reciever.hpp"

int main(int argc, char *argv[]) {
    sockaddr_in addr{AF_INET, htons(6666), {htonl(INADDR_LOOPBACK)}, {0}};
    sum_reciever rec(addr);
    while (true) {
#ifdef DEBUG
        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ipstr, INET_ADDRSTRLEN);
        debug::debug_log("Reciever") << "Trying to connect to sender at " << ipstr << ':' << ntohs(addr.sin_port) << "...\n";
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
