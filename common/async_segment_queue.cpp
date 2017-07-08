#include "async_segment_queue.h"

#include "segment.h"

using common::async_segment_queue;

async_segment_queue::async_segment_queue()  {

}

async_segment_queue::~async_segment_queue() {
    for(; segments_.size() != 0; segments_.pop_front())
        delete segments_.front();
}

void async_segment_queue::push(segment* seg) {
    std::unique_lock<std::mutex> lock(count_lock_);
    segments_.push_back(seg);
    count_cond_.notify_one();
}

common::segment* async_segment_queue::pop(std::chrono::milliseconds duration) {
    std::unique_lock<std::mutex> lock(count_lock_);
    segment* seg = nullptr;
    if(segments_.size() > 0) {
        seg = segments_.front();
        segments_.pop_front();
    } else {
        count_cond_.wait_for(lock, duration);
        if(segments_.size() > 0) {
            seg = segments_.front();
            segments_.pop_front();
        }
    }
    return seg;
}

void async_segment_queue::cancel_pop() {
    std::unique_lock<std::mutex> lock(count_lock_);
    count_cond_.notify_all();
}