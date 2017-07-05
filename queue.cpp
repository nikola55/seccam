#include "queue.h"

#include "video/segment.h"

#include <cassert>

using common::queue;

queue::queue(std::size_t max_buffer_bytes) 
    : max_buffer_bytes_(max_buffer_bytes)
    , current_buffer_bytes_(0)
    , stop_(false) {

}

queue::~queue() {
    for(; segments_.size() != 0; segments_.pop_back()) {
        delete segments_.back();
    }
}

video::segment* queue::pop_segment() {
    video::segment* segment = nullptr;
    {
        std::unique_lock<std::mutex> free_buffer_lck(free_buffer_bytes_mutex_);
        std::unique_lock<std::mutex> segments_lck(segments_available_mutex_);

        while(!stop_ || segments_.size() == 0)
            segments_available_.wait(segments_lck);
        if(stop_) {
            return nullptr;
        }
        segment = segments_.front();
        segments_.pop_front();

        current_buffer_bytes_ -= segment->size();
        assert(0 <= current_buffer_bytes_);
        free_buffer_bytes_.notify_one();
    }
    return segment;
}

void queue::push_segment(video::segment* segment) {
    std::unique_lock<std::mutex> free_buffer_lck(free_buffer_bytes_mutex_);
    std::unique_lock<std::mutex> segments_lck(segments_available_mutex_);

    while(!stop_ || (current_buffer_bytes_+segment->size()) >= max_buffer_bytes_)
        free_buffer_bytes_.wait(free_buffer_lck);
    if(stop_) {
        delete segment;
        return;
    }

    segments_.push_back(segment);
    current_buffer_bytes_ += segment->size();
    segments_available_.notify_one();
}