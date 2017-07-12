#include "http_request.h"

#include <event2/http.h>
#include <event2/buffer.h>

using net::http_request;

http_request::http_request(const std::string& method, const std::string& path)
    : method_(method)
    , path_(path)
    , data_size_(0)
    , http_request_(evhttp_request_new(&http_request::on_evhttp_request_done, this))
    , evhttp_request_done_cb_(NULL)
    , ctx_(NULL) {

}

http_request::~http_request() {
    if(NULL != http_request_)
        evhttp_request_free(http_request_);
}

void http_request::add_header(const std::string& field, const std::string& value) {
    evkeyvalq* headers = evhttp_request_get_output_headers(http_request_);
    evhttp_add_header(headers, field.c_str(), value.c_str());
}

void http_request::data(const char* buf, std::size_t sz) {
    evbuffer *evbuf = evhttp_request_get_output_buffer(http_request_);
    evbuffer_add(evbuf, buf, sz);
    data_size_+=sz;
}

void http_request::reference_data(const void* buf, std::size_t sz) {
    evbuffer *evbuf = evhttp_request_get_output_buffer(http_request_);
    evbuffer_add_reference(evbuf, buf, sz, NULL, NULL);
}

const std::string& http_request::method() const {
    return method_;
}

const std::string& http_request::path() const {
    return path_;
}

void http_request::callback(evhttp_request_done_cb cb, void* ctx) {
    evhttp_request_done_cb_ = cb;
    ctx_ = ctx;
}

void http_request::on_evhttp_request_done(evhttp_request *evreq, void *ctx) {
    http_request* http_req = static_cast<http_request*>(ctx);
    http_req->handle_on_evhttp_request_done(evreq);
}

void http_request::handle_on_evhttp_request_done(evhttp_request *evreq) {
    evhttp_request_done_cb_(evreq, ctx_);
}

evhttp_request* http_request::request() {
    evhttp_request* req = http_request_;
    http_request_ = NULL;
    return req;
}
