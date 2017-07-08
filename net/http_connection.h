#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <openssl/ssl.h>

#include <string>

struct event_base;
struct evdns_base;
struct bufferevent;
struct evhttp_connection;
struct evhttp_request;

namespace net {

class http_request;
class http_response;

class http_connection {
public:
    typedef void (*connection_error_cb)(void*);
    typedef void (*http_request_ready_cb)(http_request*, http_response*, void*);
public:
    http_connection(const std::string& base_uri, 
                    event_base* evbase, 
                    evdns_base* evdns, 
                    SSL_CTX* ssl_ctx,
                    connection_error_cb on_close_cb,
                    void* ctx
                    );
    ~http_connection();
public:
    // transfers ownership of @arg1
    void make_request(http_request* req, http_request_ready_cb cb, void* ctx);
private:
    http_connection(const http_connection&) = delete;
    void operator=(const http_connection&) = delete;
private:
    static void on_connection_close(evhttp_connection*, void* ctx);
    void handle_on_connection_close();

    static void on_evhttp_request_done(evhttp_request *, void *);

private:
    const std::string& base_uri_;
    event_base* evbase_;
    evdns_base* evdns_; 
    bufferevent* bev_;
    evhttp_connection* evconnection_;
    SSL_CTX* ssl_ctx_;
    connection_error_cb on_close_cb_;
    void* ctx_;
};

}

#endif