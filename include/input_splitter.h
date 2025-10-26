#ifndef INPUT_SPLITTER_H
#define INPUT_SPLITTER_H

#include <vector>
#include <string>

class InputSplitter {
public:
    std::vector<std::string> split(const std::string& filePath, int numSplits);
};

#endif
