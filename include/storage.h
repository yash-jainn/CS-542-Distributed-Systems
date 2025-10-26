#ifndef STORAGE_H
#define STORAGE_H

#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include<bits/stdc++.h>

class LocalStorage {
public:
    std::unordered_map<std::string, std::vector<std::string>> buffer;

    void write(const std::string& key, const std::string& value);
    std::vector<std::string> read(const std::string& key);
    void clear();
};

class GlobalStorage {
public:
    std::unordered_map<std::string, std::vector<std::string>> data;
    std::mutex mtx;

    void write(const std::string& key, const std::string& value);
    std::vector<std::string> read(const std::string& key);
    void print();
};

#endif
