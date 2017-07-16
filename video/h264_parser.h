#ifndef H264_PARSER_H
#define H264_PARSER_H

#include <cstdint>

struct AVCodecParserContext;
struct AVCodecContext;
struct AVPacket;

namespace video {

class data_source;

class h264_parser {
public:
    h264_parser();
    ~h264_parser();
private:
    h264_parser(const h264_parser&) = delete;
    void operator=(const h264_parser&) = delete;
private:
    friend class h264_decoder;
    void initialize(AVCodecContext* codec_ctx);
public:
    void attach_source(data_source* src);
public:
    int read_packet(AVPacket*);
private:
    AVCodecParserContext *av_parser_;
    enum { buf_size = 4096 };
    std::uint8_t* buf_;
    std::uint8_t* datap_;
    std::size_t sz_;
    AVCodecContext* codec_ctx_;
    data_source* src_;
    bool eof_reached_;
};

}

#endif