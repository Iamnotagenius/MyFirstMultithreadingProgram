#ifndef __CONNECTION_HPP
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <set>
#include <sys/poll.h>
class sum_sender {
    private:
        int sockfd;
        int port;
        std::mutex queue_mutex;
        std::condition_variable queue_condition;
        std::queue<int> to_send;
        std::set<int> fds;

        std::vector<int> accept_new_connections();
    public:
        sum_sender(const char* port, int max_connections = 5);
        void handle_connections(int sigfd, const std::atomic_bool& should_stop);
        void send_sum(int sum);
        void notify_eof();
        ~sum_sender();
};
#endif
