#include <condition_variable>
#include <mutex>
#include <queue>
#include <set>
#include <sys/poll.h>
#ifndef __INFOTEX_CONNECTION_HPP
class connection {
    private:
        int sockfd;
        int port;
        std::mutex queue_mutex;
        std::condition_variable queue_condition;
        std::queue<int> to_send;
        std::set<int> fds;

        void accept_new_connections();
    public:
        connection(int port, int max_connections = 5);
        void handle_connections();
        void send_sum(int sum);
        ~connection();
};


class connection_exception : std::exception {
    private:
        int error_code;
    public:
        connection_exception(int error_code) : error_code(error_code) {}
        const char* what() const noexcept;
};
#endif
