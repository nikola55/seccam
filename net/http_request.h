#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <cstdint>
#include <cstddef>

struct evhttp_request;

namespace net {

class http_request {
private:
    typedef void (*evhttp_request_done_cb)(evhttp_request *, void *);
public:
    http_request(const std::string& method, const std::string& path);
    ~http_request();
private:
    http_request(const http_request&) = delete;
    void operator=(const http_request&) = delete;
public:
    void add_header(const std::string& field, const std::string& value);
    void data(const char* buf, std::size_t sz);
    void reference_data(const void*, std::size_t);
public:
    const std::string& method() const;
    const std::string& path() const;
private:
    friend class http_connection;
    void callback(evhttp_request_done_cb, void*);
    
    evhttp_request* request();
    void request(evhttp_request*);
private:
    static void on_evhttp_request_done(evhttp_request *, void *);
    void handle_on_evhttp_request_done(evhttp_request *);
private:
    std::string method_;
    std::string path_;
    std::uint32_t data_size_;
    evhttp_request* http_request_;
    evhttp_request_done_cb evhttp_request_done_cb_;
    void* ctx_;
};

}

#endif
