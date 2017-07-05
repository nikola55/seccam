#include "segment.h"

using video::segment;

const std::uint8_t* segment::buffer() const {
    if(0 != buffer_.size()) {
        return &buffer_[0];
    } else {
        return 0;
    }
}

std::size_t segment::size() const {
    return buffer_.size();
}

void segment::insert(const std::uint8_t* buf, std::size_t sz) {
    buffer_.insert(buffer_.end(), &buf[0], &buf[sz]);
}