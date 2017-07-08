#ifndef ASYNC_SEGMENT_QUEUE_H
#define ASYNC_SEGMENT_QUEUE_H

#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace common {

class segment;

class async_segment_queue {
public:
    async_segment_queue();
    ~async_segment_queue();
private:
    async_segment_queue(const async_segment_queue&) = delete;
    void operator=(const async_segment_queue&) = delete;
public:
    void push(segment*);
    segment* pop(std::chrono::milliseconds duration);
    void cancel_pop();
private:
    std::list<segment*> segments_;
    std::mutex count_lock_;
    std::condition_variable count_cond_;
};

}

#endif