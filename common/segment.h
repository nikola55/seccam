#ifndef SEGMENT_H
#define SEGMENT_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace common {

class segment {
public:
    const std::uint8_t* buffer() const;
    std::size_t size() const;
public:
    void insert(const std::uint8_t* buf, std::size_t sz);
private:
    std::vector<std::uint8_t> buffer_;
};

}

#endif