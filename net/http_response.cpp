#include "http_response.h"

#include <event2/http.h>
#include <event2/buffer.h>

#include <cassert>

using net::http_response;

http_response::http_response(evhttp_request* evreq) 
    : evreq_(evreq)
    , buffer_read_(false) {
    
}

http_response::~http_response() {
}

int http_response::response_code() const {
    return evhttp_request_get_response_code(evreq_);
}

const char* http_response::response_phrase() const {
    return evhttp_request_get_response_code_line(evreq_);
}

const std::vector<uint8_t>& http_response::data() {
    if(!buffer_read_) {
        evbuffer* buf = evhttp_request_get_input_buffer(evreq_);
        std::size_t sz = evbuffer_get_length(buf);
        buffer_.resize(sz);
        int drained = evbuffer_copyout(buf, &buffer_[0], sz);
        assert(sz == drained);
        buffer_read_ = true;
    }
    return buffer_;
}