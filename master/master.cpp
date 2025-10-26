// master/master.cpp
#include <bits/stdc++.h>
#include <filesystem>
#include <thread>
#include <atomic>
#include <chrono>
#include "../include/channel.h"
#include "network.h"  // include for linking; or link separately
using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

// Simple helpers
void ensure_dirs() {
    fs::create_directories("tasks");
    fs::create_directories("intermediate");
    fs::create_directories("reduce_tasks");
    fs::create_directories("output");
}

vector<string> split_file_lines(const string &path) {
    vector<string> lines;
    ifstream in(path);
    string line;
    while (getline(in, line)) if (!line.empty()) lines.push_back(line);
    return lines;
}

void write_tasks_from_input(const string &input, int num_splits) {
    auto lines = split_file_lines(input);
    if (lines.empty()) {
        cerr << "[master] input empty or missing\n"; return;
    }
    fs::create_directories("tasks");
    int total = lines.size();
    int base = total / num_splits;
    int rem = total % num_splits;
    int idx = 0;
    for (int i=0;i<num_splits;i++){
        int cnt = base + (i < rem ? 1 : 0);
        ofstream fout("tasks/split_" + to_string(i) + ".txt");
        for(int j=0;j<cnt && idx < total;j++, idx++) fout << lines[idx] << "\n";
        fout.close();
    }
}

// In thread-mode: each worker thread will have an inbox queue + channel
struct ThreadWorker {
    int id;
    ThreadQueue inbox;
    std::mutex mtx;
    std::condition_variable cv;
    atomic<bool> running{true};
    shared_ptr<Channel> channel; // points to ThreadChannel(inbox,mtx,cv)
};

//////////////////////
// Map/Reduce logic //
//////////////////////

// helper to run map on split_data JSON
unordered_map<string,long long> map_from_text(const string &text) {
    unordered_map<string,long long> local;
    std::istringstream iss(text);
    string w;
    while (iss >> w) {
        // normalize
        while (!w.empty() && ispunct((unsigned char)w.back())) w.pop_back();
        while (!w.empty() && ispunct((unsigned char)w.front())) w.erase(w.begin());
        for (auto &c : w) c = tolower(c);
        if (!w.empty()) local[w] += 1;
    }
    return local;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string mode = "thread"; // default
    if (argc >= 2) mode = argv[1]; // "thread" or "tcp"
    int num_workers = 3;
    if (argc >= 3) num_workers = stoi(argv[2]);
    string input = "data/input.txt";

    // -------------------------------------------------
// Ping mode: simple connectivity + JSON test
// -------------------------------------------------
if (mode == "ping") {
    int port = 9000;
    int server_fd = create_server_socket(port);
    if (server_fd < 0) {
        cerr << "[master] failed to create server socket\n";
        return 1;
    }

    cout << "[master] (PING MODE) listening on port " << port << "\n";

    sockaddr_in cli; socklen_t len = sizeof(cli);
    int cfd = accept(server_fd, (sockaddr*)&cli, &len);
    if (cfd < 0) {
        perror("accept");
        return 1;
    }

    auto ch = make_tcp_channel(cfd);
    cout << "[master] Worker connected, waiting for ping...\n";

    json msg;
    ch->recv_json(msg);
    cout << "[master] received: " << msg.dump() << "\n";

    json reply;
    reply["reply"] = "pong";
    ch->send_json(reply);
    cout << "[master] sent pong\n";

    return 0;
}


    ensure_dirs();
    write_tasks_from_input(input, 3);

    cout << "[master] mode=" << mode << " num_workers=" << num_workers << "\n";

    // thread mode: spawn internal workers as threads
    vector<unique_ptr<ThreadWorker>> tworkers;
    vector<thread> tthreads;
    unordered_map<string, unordered_map<string,long long>> intermediate_store; // task_id -> map<k,v>
    mutex inter_mtx;

    if (mode == "thread") {
        // create workers and spawn thread loops
        for (int i=0;i<num_workers;i++){
            auto w = make_unique<ThreadWorker>();
            w->id = i+1;
            // make_thread_channel returns shared_ptr<Channel>
            w->channel = make_thread_channel(&w->inbox, &w->mtx, &w->cv);
            tworkers.push_back(std::move(w));
        }

        // spawn threads
        for (auto &wptr : tworkers) {
            // capture pointer to the worker object
            tthreads.emplace_back([&](ThreadWorker* w){
                // worker main loop: wait for messages via channel
                while (w->running) {
                    json msg;
                    // use unified Channel::recv_json_json()
                    if (!w->channel->recv_json(msg)) break;
                    // if empty object used as shutdown sentinel, skip
                    if (msg.empty()) continue;

                    string type = msg.value("type","");
                    if (type == "ASSIGN_MAP") {
                        string task_id = msg["task_id"];
                        string split_data = msg["split_data"];
                        auto local = map_from_text(split_data);
                        // send_json MAP_DONE back over channel
                        json done;
                        done["type"] = "MAP_DONE";
                        done["task_id"] = task_id;
                        done["intermediate"] = json::object();
                        for (auto &kv : local) done["intermediate"][kv.first] = kv.second;
                        w->channel->send_json(done);
                    } else if (type == "ASSIGN_REDUCE") {
                        string task_id = msg["task_id"];
                        // two possible payloads: either "keys" (worker should scan intermediates)
                        // or "partial" (master already aggregated partials and gave them to worker).
                        // We handle "partial" first (more efficient).
                        unordered_map<string,long long> results;
                        if (msg.contains("partial")) {
                            json partial = msg["partial"];
                            for (auto it = partial.begin(); it != partial.end(); ++it) {
                                results[it.key()] = it.value().get<long long>();
                            }
                        } else if (msg.contains("keys")) {
                            auto keys = msg["keys"];
                            // gather sums by scanning intermediate_store (shared)
                            lock_guard<mutex> lg(inter_mtx);
                            for (auto &pair : intermediate_store) {
                                for (auto &k : keys) {
                                    string keystr = k.get<string>();
                                    auto it = pair.second.find(keystr);
                                    if (it != pair.second.end()) results[keystr] += it->second;
                                }
                            }
                        }
                        json done; done["type"]="REDUCE_DONE"; done["task_id"]=task_id; done["results"]=json::object();
                        for (auto &kv: results) done["results"][kv.first] = kv.second;
                        w->channel->send_json(done);
                    }
                }
            }, wptr.get());
        }

        // Master: discover task files and assign to worker channels
        vector<string> task_files;
        for (auto &e : fs::directory_iterator("tasks")) if (e.is_regular_file()) task_files.push_back(e.path().string());
        // send_json ASSIGN_MAP messages round-robin
        int wi = 0;
        for (auto &tfile : task_files) {
            ifstream fin(tfile);
            string content((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
            json msg; msg["type"]="ASSIGN_MAP"; msg["task_id"]=tfile; msg["split_data"]=content;
            auto &w = *tworkers[wi];
            // send_json assignment
            w.channel->send_json(msg);

            // wait for MAP_DONE on worker channel
            json done;
            w.channel->recv_json(done);
            if (done["type"] == "MAP_DONE") {
                string tid = done["task_id"];
                unordered_map<string,long long> m;
                for (auto it = done["intermediate"].begin(); it != done["intermediate"].end(); ++it) {
                    m[it.key()] = it.value().get<long long>();
                }
                lock_guard<mutex> lg(inter_mtx);
                intermediate_store[tid] = std::move(m);
                cout << "[master] received MAP_DONE for " << tid << "\n";
            }
            wi = (wi+1) % (int)tworkers.size();
        }

        // Build reduce key list
        unordered_set<string> keys;
        {
            lock_guard<mutex> lg(inter_mtx);
            for (auto &p : intermediate_store) for (auto &kv : p.second) keys.insert(kv.first);
        }
        vector<string> allkeys(keys.begin(), keys.end());
        int rparts = max(1, (int)min((int)allkeys.size(), num_workers));
        // create reduce partitions (simple round-robin)
        vector<vector<string>> partitions(rparts);
        for (size_t i=0;i<allkeys.size();++i) partitions[i%rparts].push_back(allkeys[i]);

        // assign reduce tasks and wait for REDUCE_DONE
        for (int i=0;i<rparts;i++) {
            // Build a partial map to send_json to worker (Option 2 from our discussion).
            json msg; msg["type"] = "ASSIGN_REDUCE";
            msg["task_id"] = "reduce_" + to_string(i);

            // Build partial aggregated values for this partition (master-side aggregation of mapped data).
            json partial = json::object();
            {
                lock_guard<mutex> lg(inter_mtx);
                for (auto &k : partitions[i]) {
                    long long total = 0;
                    for (auto &m : intermediate_store) {
                        auto it = m.second.find(k);
                        if (it != m.second.end()) total += it->second;
                    }
                    partial[k] = total;
                }
            }
            msg["partial"] = partial;

            // send_json to a worker (round-robin)
            auto &w = *tworkers[i % (int)tworkers.size()];
            w.channel->send_json(msg);

            // wait for REDUCE_DONE
            json done;
            w.channel->recv_json(done);
            if (done["type"] == "REDUCE_DONE") {
                for (auto it = done["results"].begin(); it != done["results"].end(); ++it) {
                    cout << it.key() << "\t" << it.value() << "\n";
                }
            }
        }

        // shutdown workers (send_json empty object / stop flag)
        for (auto &w : tworkers) {
            w->running = false;
            // wake up thread by send_jsoning empty object
            w->channel->send_json(json::object());
        }
        for (auto &th : tthreads) if (th.joinable()) th.join();
        cout << "[master] thread-mode finished.\n";
        return 0;
    }

    // -------------------------------------------------
    // TCP mode: accept worker connections and protocol
    // -------------------------------------------------
    if (mode == "tcp") {
        int port = 9000;
        int server_fd = create_server_socket(port);
        if (server_fd < 0) { cerr << "[master] failed to create server socket\n"; return 1; }
        cout << "[master] listening on port " << port << "\n";

        // accept connections and keep per-connection TCPChannel (wrapped via make_tcp_channel)
        vector<shared_ptr<Channel>> worker_channels;
        // accept up to num_workers connections (or loop until enough)
        while ((int)worker_channels.size() < num_workers) {
            sockaddr_in cli; socklen_t len = sizeof(cli);
            int cfd = accept(server_fd, (sockaddr*)&cli, &len);
            if (cfd < 0) { perror("accept"); continue; }
            auto ch = make_tcp_channel(cfd);
            worker_channels.push_back(ch);
            cout << "[master] accepted worker connection\n";
        }

        // assign map tasks round-robin and collect MAP_DONE responses
        vector<string> task_files;
        for (auto &e : fs::directory_iterator("tasks")) if (e.is_regular_file()) task_files.push_back(e.path().string());
        int wi = 0;
        unordered_map<string, unordered_map<string,long long>> inter;
        for (auto &tfile : task_files) {
            ifstream fin(tfile);
            string content((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
            json msg; msg["type"]="ASSIGN_MAP"; msg["task_id"]=tfile; msg["split_data"]=content;
            auto &ch = *worker_channels[wi];
            ch.send_json(msg);
            // wait for MAP_DONE
            json done; ch.recv_json(done);
            if (done["type"] == "MAP_DONE") {
                string tid = done["task_id"];
                unordered_map<string,long long> m;
                for (auto it = done["intermediate"].begin(); it != done["intermediate"].end(); ++it) m[it.key()] = it.value().get<long long>();
                inter[tid] = m;
                cout << "[master] map_done for " << tid << "\n";
            }
            wi = (wi+1) % worker_channels.size();
        }

        // partition keys and assign reduce tasks similarly
        unordered_set<string> keys;
        for (auto &p : inter) for (auto &kv : p.second) keys.insert(kv.first);
        vector<string> allkeys(keys.begin(), keys.end());
        int rparts = max(1, (int)min((int)allkeys.size(), (int)worker_channels.size()));
        vector<vector<string>> partitions(rparts);
        for (size_t i=0;i<allkeys.size();++i) partitions[i%rparts].push_back(allkeys[i]);
        for (int i=0;i<rparts;i++) {
            json msg; msg["type"]="ASSIGN_REDUCE"; msg["task_id"]="reduce_"+to_string(i);
            // prepare partial aggregated map for this partition
            json partial = json::object();
            for (auto &k : partitions[i]) {
                long long total = 0;
                for (auto &m : inter) {
                    auto it = m.second.find(k);
                    if (it != m.second.end()) total += it->second;
                }
                partial[k] = total;
            }
            msg["partial"] = partial;

            auto &ch = *worker_channels[i % worker_channels.size()];
            ch.send_json(msg);
            json done; ch.recv_json(done);
            if (done["type"] == "REDUCE_DONE") {
                for (auto it=done["results"].begin(); it!=done["results"].end(); ++it)
                    cout << it.key() << "\t" << it.value() << "\n";
            }
        }

        cout << "[master] tcp-mode finished.\n";
        return 0;
    }

    cerr << "[master] Unknown mode: " << mode << "\n";
    return 1;
}
