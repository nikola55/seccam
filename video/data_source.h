#ifndef DATA_SOURCE_H
#define DATA_SOURCE_H

#include <cstdint>
#include <cstddef>

namespace video {

class data_source {
public:
    virtual ~data_source() { }
public:
    virtual int read_data(std::uint8_t* buf, std::size_t sz) = 0;
};

}

#endif