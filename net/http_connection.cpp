#include "http_connection.h"

#include "http_request.h"
#include "http_response.h"

#include <event2/bufferevent_ssl.h>
#include <event2/http.h>
#include <event2/dns.h>

#include <cassert>
#include <iostream>

using net::http_connection;

http_connection::http_connection(
    const std::string& base_uri, 
    event_base* evbase, 
    evdns_base* evdns, 
    SSL_CTX* ssl_ctx,
    connection_error_cb on_close_cb,
    void* ctx
    )
    : base_uri_(base_uri)
    , evbase_(evbase)
    , evdns_(evdns)
    , ssl_ctx_(ssl_ctx)
    , bev_(NULL)
    , evconnection_(NULL)
    , on_close_cb_(on_close_cb)
    , ctx_(ctx) {

    SSL* ssl = SSL_new(ssl_ctx_);
    assert(NULL != ssl);
    bev_ = bufferevent_openssl_socket_new(evbase_, -1, ssl, BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
    assert(NULL != bev_);
    bufferevent_openssl_set_allow_dirty_shutdown(bev_, 1);
    evconnection_ = evhttp_connection_base_bufferevent_new(evbase_, evdns_, bev_, base_uri.c_str(), 443);
    evhttp_connection_set_retries(evconnection_, -1);
    evhttp_connection_set_closecb(evconnection_, &http_connection::on_connection_close, this);

}

http_connection::~http_connection() {
    evhttp_connection_free(evconnection_);
}

void http_connection::on_connection_close(evhttp_connection*, void* ctx) {
    http_connection* httpcon = static_cast<http_connection*>(ctx);
    httpcon->handle_on_connection_close();
}

void http_connection::handle_on_connection_close() {
    on_close_cb_(ctx_);
}

class handler {
public:
    handler(net::http_request* request, http_connection::http_request_ready_cb cb, void* ctx);
    virtual ~handler();
public:
    void execute(net::http_response*);
private:
    handler(const handler&) = delete;
    void operator=(const handler&) = delete;
private:
    net::http_request* request_;
    http_connection::http_request_ready_cb cb_;
    void* ctx_;
};

handler::handler(net::http_request* request, http_connection::http_request_ready_cb cb, void* ctx)
    : request_(request)
    , cb_(cb)
    , ctx_(ctx) {
}

handler::~handler() {

}

void handler::execute(net::http_response* res) {
    if(0 == res) {
        cb_(request_, 0, ctx_);
    } else {
        cb_(request_, res, ctx_);
    }
}

void http_connection::on_evhttp_request_done(evhttp_request *evreq, void *ctx) {
    handler* hnd = static_cast<handler*>(ctx);
    if(NULL == evreq) {
        hnd->execute(0);
    } else {
        // evhttp_request_own(evreq);
        net::http_response* resp = new net::http_response(evreq);
        hnd->execute(resp);
    }
    delete hnd;
}

namespace {
evhttp_cmd_type str_to_evmethod(const std::string& str_method) {
    if(str_method == "POST") {
        return EVHTTP_REQ_POST;
    } else if (str_method == "GET") {
        return EVHTTP_REQ_GET;
    } else {
        assert(NULL == "Invalid method");
    }
}
}

void http_connection::make_request(http_request* req, http_request_ready_cb cb, void* ctx) {
    req->callback(&http_connection::on_evhttp_request_done, new handler(req,cb,ctx));
    evhttp_make_request(evconnection_, req->request(), str_to_evmethod(req->method()), req->path().c_str());
}