#ifndef QUEUE_H
#define QUEUE_H

#include <cstddef>
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace video { 
class segment; 
}

namespace common {

class queue {
public:
    queue(std::size_t max_buffer_bytes = (1024*1024*10));
    ~queue();
private:
    queue(const queue&);
    void operator=(const queue&);
public:
    video::segment* pop_segment();
    void push_segment(video::segment* segment);
private:
    std::size_t max_buffer_bytes_;
    std::size_t current_buffer_bytes_;
    std::atomic<bool> stop_;
    std::list<video::segment*> segments_;
    std::condition_variable segments_available_;
    std::mutex segments_available_mutex_;
    std::condition_variable free_buffer_bytes_;
    std::mutex free_buffer_bytes_mutex_;
};

}

#endif