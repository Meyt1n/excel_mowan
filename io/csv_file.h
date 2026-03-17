#pragma once

#include <string>
#include <vector>

namespace emw {

class CsvFile {
public:
    static std::vector<std::string> SplitLine(const std::string& line);
    static std::string JoinLine(const std::vector<std::string>& fields);
};

}
