#include "queue.h"

#include "segment.h"

#include <cassert>
#include <iostream>
#include <chrono>

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

common::segment* queue::pop_segment() {
    segment* segment = nullptr;
    {
        std::unique_lock<std::mutex> segments_lck(segments_available_mutex_);

        while(!stop_ && segments_.size() == 0) {
            // std::cout << "QUEUE queue::pop_segment no available segments" << std::endl;
            segments_available_.wait_for(segments_lck, std::chrono::milliseconds(100));
        }
        if(stop_) {
            return nullptr;
        }
        segment = segments_.front();
        segments_.pop_front();

        std::unique_lock<std::mutex> free_buffer_lck(free_buffer_bytes_mutex_);
        current_buffer_bytes_ -= segment->size();
        assert(0 <= current_buffer_bytes_);
        free_buffer_bytes_.notify_one();
        std::cout << "QUEUE queue::pop_segment available segment" << std::endl;
    }
    return segment;
}

void queue::push_segment(segment* segment) {
    std::unique_lock<std::mutex> free_buffer_lck(free_buffer_bytes_mutex_);
    while(!stop_ && (current_buffer_bytes_+segment->size()) >= max_buffer_bytes_) {
        std::cout << "QUEUE queue::push_segment stop_=" << stop_  << std::endl
                  << " current size is " << (current_buffer_bytes_+segment->size()) << std::endl 
                  << " max buffer space is " << max_buffer_bytes_ << std::endl;

        free_buffer_bytes_.wait_for(free_buffer_lck, std::chrono::milliseconds(100));
    }
    if(stop_) {
        delete segment;
        return;
    }

    segments_.push_back(segment);
    current_buffer_bytes_ += segment->size();
    std::unique_lock<std::mutex> segments_lck(segments_available_mutex_);
    segments_available_.notify_one();
    std::cout << "QUEUE queue::push_segment segment inserted" << std::endl;
}