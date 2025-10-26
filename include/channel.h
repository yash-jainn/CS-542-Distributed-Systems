#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Abstract communication interface between master and worker
class Channel {
public:
    virtual bool send_json(const json &msg) = 0;
    virtual bool recv_json(json &msg) = 0;
    virtual ~Channel() {}
};

// -------------------------
// Thread-based channel
// -------------------------
struct ThreadQueue {
    std::queue<json> messages;
    std::mutex mtx;
    std::condition_variable cv;
};

class ThreadChannel : public Channel {
    ThreadQueue *peer_inbox;
    std::mutex *peer_mtx;
    std::condition_variable *peer_cv;

public:
    ThreadChannel(ThreadQueue *peer, std::mutex *m, std::condition_variable *c)
        : peer_inbox(peer), peer_mtx(m), peer_cv(c) {}

    bool send_json(const json &msg) override {
        std::unique_lock<std::mutex> lock(*peer_mtx);
        peer_inbox->messages.push(msg);
        lock.unlock();
        peer_cv->notify_one();
        return true;
    }

    bool recv_json(json &msg) override {
        std::unique_lock<std::mutex> lock(peer_inbox->mtx);
        peer_inbox->cv.wait(lock, [&]{ return !peer_inbox->messages.empty(); });
        msg = peer_inbox->messages.front();
        peer_inbox->messages.pop();
        return true;
    }
};

// -------------------------
// TCP-based channel
// -------------------------
#include "network.h"

class TCPChannel : public Channel {
    TCPConnection conn;
public:
    TCPChannel(int fd) : conn(fd) {}
    bool send_json(const json &msg) override { return conn.send_json(msg); }
    bool recv_json(json &msg) override { return conn.recv_json(msg); }
};

// Factory functions
// Factory helpers
#include <memory>
std::shared_ptr<Channel> make_thread_channel(ThreadQueue *peer, std::mutex *m, std::condition_variable *c);
std::shared_ptr<Channel> make_tcp_channel(int fd);
