#ifndef API_FILE_H
#define API_FILE_H

#include <string>

class api_file {
public:
    int timestamp;
    std::string filename;
    std::string path;
    int size;
    std::string last_modified;
};

#endif