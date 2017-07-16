#include "file_data_source.h"

#include <cassert>

using video::file_data_source;

file_data_source::file_data_source(const std::string& path)
    : fp_(NULL) {

    fp_ = std::fopen(path.c_str(), "r");
    assert(NULL != fp_);
}

file_data_source::~file_data_source() {
    std::fclose(fp_);
}

int file_data_source::read_data(std::uint8_t* buf, std::size_t sz) {
    ssize_t rd = fread(buf, 1, sz, fp_);
    return rd;
}