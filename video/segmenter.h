#ifndef SEGMENTER_H
#define SEGMENTER_H

#include <list>

struct AVPacket;

namespace common {
    class segment;
}

namespace video {

class segmenter {
public:
    typedef void (*on_segment_ready_cb)(common::segment*, void*);
    typedef void (*on_eof_cb)(void*);
public:
    segmenter(on_segment_ready_cb handle_on_segment_ready, on_eof_cb handle_on_eof, void* ctx);
    ~segmenter();
private:
    segmenter(const segmenter&);
    void operator=(const segmenter&);
public:
    void on_packet(AVPacket*);
    void on_segment_end();
    void on_eof();
private:
    common::segment* cseg_;
    on_segment_ready_cb handle_on_segment_ready_;
    on_eof_cb handle_on_eof_;
    void* ctx_;
};

}

#endif