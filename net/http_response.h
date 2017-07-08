#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

struct evhttp_request;

#include <cstdint>
#include <vector>

namespace net {

class http_response {
public:
    http_response(evhttp_request* evreq);
    ~http_response();
public:
    int response_code() const;
    const char* response_phrase() const;
public:
    const std::vector<uint8_t>& data();
private:
    http_response(const http_response&) = delete;
    void operator=(const http_response&) = delete;
private:
    evhttp_request* evreq_;
    bool buffer_read_;
    std::vector<uint8_t> buffer_;
};

}

#endif