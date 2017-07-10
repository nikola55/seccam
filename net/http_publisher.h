#ifndef HTTP_PUBLISHER_H
#define HTTP_PUBLISHER_H

#include "api_file.h"

#include <openssl/ssl.h>

#include <string>
#include <map>
#include <list>

struct event_base;
struct evdns_base;
struct event;

namespace Json {
    class Value;
}

namespace common {
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
                    int read_segment_fd,
                    connection_event_cb on_connection_ready,
                    connection_event_cb on_connection_error,
                    connection_event_cb on_last_request_sent,
                    void* ctx
                    );
    ~http_publisher();
private:
    static void on_segments(int, short what, void *ctx);
    void handle_on_segments();
private:
    http_publisher(const http_publisher&) = delete;
    void operator=(const http_publisher&) = delete;
private:
    static void on_api_connection_lost(void*);
    void handle_on_api_connection_lost();

    static void on_file_upload_connection_lost(void*);
    void handle_on_file_upload_connection_lost();
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
    int read_segment_fd_;
    event* read_segments_event_;
    std::list<common::segment*> segment_list_;
    connection_event_cb on_connection_ready_;
    connection_event_cb on_connection_error_;
    connection_event_cb on_last_request_sent_;
    void* ctx_;
    http_connection* api_;
    http_connection* file_upload_;
    std::map<int, api_file> files_by_timestamp_;
    long files_size_;
    bool last_segment_;
};

}

#endif
