#include "storage.h"
#include <iostream>

void LocalStorage::write(const std::string& key, const std::string& value) {
    buffer[key].push_back(value);
}

std::vector<std::string> LocalStorage::read(const std::string& key) {
    return buffer[key];
}

void LocalStorage::clear() {
    buffer.clear();
}

void GlobalStorage::write(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mtx);
    data[key].push_back(value);
}

std::vector<std::string> GlobalStorage::read(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx);
    return data[key];
}

void GlobalStorage::print() {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& kv : data) {
        std::cout << kv.first << " : ";
        for (auto& v : kv.second)
            std::cout << v << " ";
        std::cout << std::endl;
    }
}
