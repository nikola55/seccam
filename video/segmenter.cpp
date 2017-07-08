#include "segmenter.h"

#include "common/segment.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <cassert>

using video::segmenter;

segmenter::segmenter(on_segment_ready_cb handle_on_segment_ready, on_eof_cb handle_on_eof, void* ctx)
    : cseg_(new common::segment)
    , handle_on_segment_ready_(handle_on_segment_ready)
    , handle_on_eof_(handle_on_eof)
    , ctx_(ctx) {

}

segmenter::~segmenter() {
    delete cseg_;
}

void segmenter::on_packet(AVPacket* packet) {
    assert(NULL != cseg_);
    cseg_->insert(packet->data, packet->size);
}

void segmenter::on_segment_end() {
    assert(NULL != cseg_);
    handle_on_segment_ready_(cseg_, ctx_); // transfer ownership
    cseg_ = new common::segment;
}

void segmenter::on_eof() {
    handle_on_segment_ready_(cseg_, ctx_); // transfer ownership
    handle_on_eof_(ctx_);
    cseg_ = 0;
}