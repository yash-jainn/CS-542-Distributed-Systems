#include "channel.h"

std::shared_ptr<Channel> make_thread_channel(ThreadQueue *peer, std::mutex *m, std::condition_variable *c) {
    return std::make_shared<ThreadChannel>(peer, m, c);
}

std::shared_ptr<Channel> make_tcp_channel(int fd) {
    return std::make_shared<TCPChannel>(fd);
}
