#include "mapper.h"
#include <sstream>

std::vector<KeyValue> Mapper::map(const std::string& chunk) {
    std::vector<KeyValue> kvPairs;
    std::stringstream ss(chunk);
    std::string word;

    while (ss >> word)
        kvPairs.push_back({word, 1});

    return kvPairs;
}
