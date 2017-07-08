#ifndef HTTP_PUBLISHER_H
#define HTTP_PUBLISHER_H

#include "api_file.h"

#include <openssl/ssl.h>

#include <string>
#include <map>

struct event_base;
struct evdns_base;

namespace Json {
    class Value;
}

namespace common {
    class async_segment_queue;
    class segment;
}

namespace net {

class http_connection;
class http_request;
class http_response;

class http_publisher {
private:
    typedef void (*connection_event_cb)(void*);
public:
    http_publisher( const std::string& base_uri,
                    const std::string& file_upload_uri,
                    const std::string& bearer,
                    event_base* evbase,
                    evdns_base* evdns,
                    SSL_CTX* ssl_ctx,
                    common::async_segment_queue& queue,
                    connection_event_cb on_connection_ready,
                    connection_event_cb on_connection_error,
                    void* ctx
                    );
    ~http_publisher();
private:
    http_publisher(const http_publisher&) = delete;
    void operator=(const http_publisher&) = delete;
private:
    static void on_connection_lost(void*);
    void handle_on_connection_lost();
private:
    void list_folders();
    void list_folders_continue(const std::string& cursor);
private:
    void list_app_folder();
    void list_app_folder_continue(const std::string& cursor);

    void create_app_folder();
private:
    void send_segment(common::segment* seg);
private:
    void pop_segment();
private:
    void on_enumerate_files_complete();
private:
    static void on_list_folders_complete(http_request*, http_response*, void*);
    void handle_on_list_folders_complete(http_request*, http_response*);

    static void on_create_folder_complete(http_request*, http_response*, void*);
    void handle_on_create_folder_complete(http_request*, http_response*);

    static void on_list_app_folder_complete(http_request*, http_response*, void*);
    void handle_on_list_app_folder_complete(http_request*, http_response*);

    static void on_send_segment_complete(http_request*, http_response*, void*);
    void handle_on_send_segment_complete(http_request*, http_response*);
private:
    void append_to_files(Json::Value& entries);
private:
    std::string base_uri_;
    std::string file_upload_uri_;
    std::string bearer_;
    event_base* evbase_;
    evdns_base* evdns_;
    SSL_CTX* ssl_ctx_;
    common::async_segment_queue& queue_;
    connection_event_cb on_connection_ready_;
    connection_event_cb on_connection_error_;
    void* ctx_;
    http_connection* api_;
    http_connection* file_upload_;
    std::map<int, api_file> files_by_timestamp_;
    long files_size_;
    bool exit_pending_;
};

}

#endif