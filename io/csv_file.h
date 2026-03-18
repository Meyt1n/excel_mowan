#pragma once

#include <string>
#include <vector>

using namespace std;


namespace emw {

class CsvFile {
public:
    static vector<string> SplitLine(const string& line);
    static string JoinLine(const vector<string>& fields);
};

}
