#ifndef REDUCER_H
#define REDUCER_H

#include "common.h"
#include <vector>
#include <string>
#include <map>

class Reducer {
public:
    std::map<std::string, int> reduce(const std::vector<KeyValue>& intermediate);
};

#endif
