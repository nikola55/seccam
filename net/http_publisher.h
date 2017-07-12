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
    bool process_upload_response(Json::Value& resp);
private:
    static void on_list_folders_complete(http_request*, http_response*, void*);
    void handle_on_list_folders_complete(http_request*, http_response*);

    static void on_create_folder_complete(http_request*, http_response*, void*);
    void handle_on_create_folder_complete(http_request*, http_response*);

    static void on_list_app_folder_complete(http_request*, http_response*, void*);
    void handle_on_list_app_folder_complete(http_request*, http_response*);

    class on_segment_sent_handler;
    static void on_send_segment_complete(http_request*, http_response*, void*);
    void handle_on_send_segment_complete(http_request*, http_response*, Json::Value* json_arg, common::segment* seg);

    class retry_timer_handler;
    static void on_retry_timer_expired(int, short what, void*);
    void handle_on_retry_expired(int retry_cnt, Json::Value* json_arg, common::segment* seg);

    class on_send_segment_retry_complete_handler;
    static void on_send_segment_retry_complete(http_request* req, http_response* res, void* ctx);
    void handle_on_send_segment_retry_complete(http_request* req, http_response* res, int retry_cnt, Json::Value* json_arg, common::segment* seg);

private:
    void start_retry_timer(Json::Value* json_arg, common::segment* seg, int retry_count);
private:
    bool validate_response(http_response* resp);
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
    int initial_retry_sec_;
    int max_retry_count_;
private:
    enum http_state {
        initializing,
        listing_root_folder,
        listing_root_folder_continue,
        creating_app_folder,
        listing_app_folder,
        listing_app_folder_continue,
        generic_http_error,
        starting_video_thread,
        idle,
        popping_segment,
        sending_segment,
        sending_segment_retry_timer,
        sending_segment_retry,
        terminating_video
    };
private:
    http_state state_;
};

}

#endif
