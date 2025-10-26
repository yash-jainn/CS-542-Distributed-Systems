#ifndef MAPPER_H
#define MAPPER_H

#include "common.h"
#include <vector>
#include <string>

class Mapper {
public:
    std::vector<KeyValue> map(const std::string& chunk);
};

#endif
