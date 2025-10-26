// worker/worker.cpp
#include <bits/stdc++.h>
#include "../include/network.h"
using namespace std;
using json = nlohmann::json;

unordered_map<string,long long> map_from_text(const string &text) {
    unordered_map<string,long long> local;
    istringstream iss(text);
    string w;
    while (iss >> w) {
        while (!w.empty() && ispunct((unsigned char)w.back())) w.pop_back();
        while (!w.empty() && ispunct((unsigned char)w.front())) w.erase(w.begin());
        for (auto &c : w) c = tolower(c);
        if (!w.empty()) local[w] += 1;
    }
    return local;
}
int main(int argc, char** argv) {
    // ----------------------------
    // PING MODE for connectivity test
    // ----------------------------
    if (argc == 4 && string(argv[1]) == "ping") {
        string ip = argv[2];
        int port = stoi(argv[3]);

        int sock = connect_to(ip, port);
        if (sock < 0) {
            cerr << "[worker] (PING MODE) failed to connect to master\n";
            return 1;
        }

        TCPConnection conn(sock);
        cout << "[worker] (PING MODE) connected to " << ip << ":" << port << "\n";

        json msg; msg["hello"] = "ping";
        conn.send_json(msg);
        cout << "[worker] sent ping\n";

        json reply;
        if (conn.recv_json(reply))
            cout << "[worker] received: " << reply.dump() << "\n";
        else
            cerr << "[worker] failed to receive reply\n";

        return 0;
    }

    // ----------------------------
    // NORMAL MAPREDUCE WORKER MODE
    // ----------------------------
    string master_ip = "127.0.0.1";
    int master_port = 9000;
    if (argc >= 2) master_ip = argv[1];
    if (argc >= 3) master_port = stoi(argv[2]);

    int sock = connect_to(master_ip, master_port);
    if (sock < 0) {
        cerr << "[worker] failed to connect to master\n";
        return 1;
    }

    TCPConnection conn(sock);
    cout << "[worker] connected to master\n";

    json msg;
    while (conn.recv_json(msg)) {
        string type = msg.value("type", "");
        if (type == "ASSIGN_MAP") {
            string task_id = msg["task_id"];
            string data = msg["split_data"];
            auto local = map_from_text(data);

            json done;
            done["type"] = "MAP_DONE";
            done["task_id"] = task_id;
            done["intermediate"] = json::object();
            for (auto &kv : local)
                done["intermediate"][kv.first] = kv.second;
            conn.send_json(done);
        } else if (type == "ASSIGN_REDUCE") {
            auto partial = msg["partial"];
            unordered_map<string, long long> results;
            for (auto it = partial.begin(); it != partial.end(); ++it)
                results[it.key()] = it.value().get<long long>();

            json done;
            done["type"] = "REDUCE_DONE";
            done["task_id"] = msg["task_id"];
            done["results"] = results;
            conn.send_json(done);
        } else {
            // ignore or handle heartbeats later
        }
    }

    cout << "[worker] connection closed\n";
    return 0;
}
