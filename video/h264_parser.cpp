#include "h264_parser.h"

#include "data_source.h"

#include <cassert>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
}

using video::h264_parser;

h264_parser::h264_parser() 
    : av_parser_(av_parser_init(AV_CODEC_ID_H264))
    , buf_(new std::uint8_t[buf_size + AV_INPUT_BUFFER_PADDING_SIZE])
    , datap_(buf_+buf_size)
    , sz_(0)
    , codec_ctx_(NULL)
    , src_(nullptr)
    , eof_reached_(false) {

    assert(NULL != av_parser_);
    assert(NULL != buf_);

    std::memset(buf_+buf_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

}

h264_parser::~h264_parser() {
    delete buf_;
    av_parser_close(av_parser_);
}

void h264_parser::initialize(AVCodecContext* codec_ctx) {
    assert(NULL == codec_ctx_);
    assert(NULL != codec_ctx);
    codec_ctx_ = codec_ctx;
}

void h264_parser::attach_source(data_source* src) {
    assert(nullptr == src_);
    assert(nullptr != src);
    src_ = src;
}

/*
    Returns 1 on success
    Returns 0 on EOF
    Returns < 0 on error
    
*/

int h264_parser::read_packet(AVPacket* pkt) {

    if(eof_reached_) {
        return 0;
    }

    while(true) {

        if(0 == sz_) {
            datap_ = buf_;
            std::size_t rd = src_->read_data(datap_, buf_size);
            if(rd == 0) {
                int ret = av_parser_parse2(av_parser_, codec_ctx_, &pkt->data, &pkt->size,
                                           NULL, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0
                                          );
                if(ret < 0) {
                    return -1;
                } else {
                    eof_reached_ = true;
                    if(pkt->size != 0) {
                        return 1;
                    } else {
                        return 0;
                    }
                }
            } else if(rd < 0) {
                // TODO: check if we have to drain parser
                return -1;
            }
            sz_ = rd;
        }

        int ret = av_parser_parse2(av_parser_, codec_ctx_, &pkt->data, &pkt->size,
                                   datap_, sz_, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0
                                   );

        if(ret < 0) {
            return -1;
        } 

        datap_ += ret;
        sz_ -= ret;

        if(pkt->size != 0) {
            return 1;
        }
    }

}