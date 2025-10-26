#include "reducer.h"

std::map<std::string, int> Reducer::reduce(const std::vector<KeyValue>& intermediate) {
    std::map<std::string, int> counts;

    for (const auto& kv : intermediate)
        counts[kv.key] += kv.value;

    return counts;
}
