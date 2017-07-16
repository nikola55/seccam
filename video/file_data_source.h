#ifndef FILE_DATA_SOURCE_H
#define FILE_DATA_SOURCE_H

#include "data_source.h"

#include <string>

#include <cstdio>

namespace video {

class file_data_source : public data_source {
public:
    file_data_source(const std::string& path);
    ~file_data_source();
private:
    file_data_source(const file_data_source&) = delete;
    void operator=(const file_data_source&) = delete;
public:
    int read_data(std::uint8_t* buf, std::size_t sz);
private:
    std::FILE* fp_;
};

}

#endif