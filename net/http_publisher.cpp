#include "http_publisher.h"

#include "logging/log.h"

#include "common/segment.h"

#include "http_connection.h"
#include "http_request.h"
#include "http_response.h"

#include <json/json.h>
#include <event2/event.h>

#include <unistd.h>

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cassert>
#include <cstring>

using net::http_publisher;

http_publisher::http_publisher(
    const std::string& base_uri,
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
    )
    : base_uri_(base_uri)
    , file_upload_uri_(file_upload_uri)
    , bearer_(bearer)
    , evbase_(evbase)
    , evdns_(evdns)
    , ssl_ctx_(ssl_ctx)
    , read_segment_fd_(read_segment_fd)
    , read_segments_event_(NULL)
    , on_connection_ready_(on_connection_ready)
    , on_connection_error_(on_connection_error)
    , on_last_request_sent_(on_last_request_sent)
    , ctx_(ctx)
    , api_(nullptr)
    , file_upload_(nullptr)
    , files_size_(0)
    , last_segment_(false)
    , state_(initializing)
    , initial_retry_sec_(2)
    , max_retry_count_(5) {


    try {
        api_ = new http_connection(base_uri_, evbase_, evdns_, ssl_ctx_, &http_publisher::on_api_connection_lost, this);
    } catch (const std::runtime_error& err) {
        LOG(common::log::err) << "Cannot create http_connection to " << base_uri_ << common::log::end;
        return;
    }
    
    try {
        file_upload_ = new http_connection(file_upload_uri_, evbase_, evdns_, ssl_ctx_, &http_publisher::on_file_upload_connection_lost, this);
    } catch (const std::runtime_error& err) {
        delete api_;
        LOG(common::log::err) << "Cannot create http_connection to " << file_upload_uri << common::log::end;
        return;
    }

    read_segments_event_ = event_new(evbase_, read_segment_fd_, EV_READ|EV_PERSIST, &http_publisher::on_segments, this);
    assert(NULL != read_segments_event_);
    event_add(read_segments_event_, NULL);

    list_folders();

}

http_publisher::~http_publisher() {

    for(; segment_list_.size() != 0; segment_list_.pop_front())
        delete segment_list_.front();

    event_free(read_segments_event_);
    delete file_upload_;
    delete api_;
}

void http_publisher::on_segments(int, short what, void *ctx) {
    assert(EV_READ == what);
    static_cast<http_publisher*>(ctx)->handle_on_segments();
}

void http_publisher::handle_on_segments() {
    if(state_ != idle && state_ != popping_segment && state_ != sending_segment) {
        assert(state_ == idle ||
               state_ == popping_segment ||
               state_ == sending_segment);
        return;
    }
    LOG(common::log::info) << "reading segment" << common::log::end;
    bool segments_added = false;
    while(true) {
        std::uintptr_t seg_ptr;
        ssize_t ret = read(read_segment_fd_, &seg_ptr, sizeof(seg_ptr));
        if(ret == sizeof(seg_ptr)) {
            LOG(common::log::info) << "adding segment" << common::log::end;
            common::segment* seg = reinterpret_cast<common::segment*>(seg_ptr);
            segment_list_.push_back(seg);
            segments_added = true;
        } else {
            if(ret == 0) {
                // EOF
                LOG(common::log::err) << "EOF" << common::log::end;
                assert(false);
                return;
            } else if (ret < 0) {
                if(errno == EINTR) {
                    continue;
                } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    break;
                } else {
                    // fatal error
                    int err = errno;
                    LOG(common::log::err) << "errno=" << std::strerror(err) << common::log::end;
                    assert(false);
                    return;
                }
            } else {
                // TODO: incomplete read
                assert(false);
                return;
            }
        }
    }
    if(segments_added && state_ == idle) {
        pop_segment();
    }
}

void http_publisher::on_api_connection_lost(void* ctx) {
    http_publisher* httppub = static_cast<http_publisher*>(ctx);
    httppub->handle_on_api_connection_lost();
}

void http_publisher::handle_on_api_connection_lost() {
    LOG(common::log::info) << " refresh api connection " << common::log::end;
    // delete api_;
    // api_ = new http_connection(base_uri_, evbase_, evdns_, ssl_ctx_, &http_publisher::on_api_connection_lost, this);
}

void http_publisher::on_file_upload_connection_lost(void* ctx) {
    http_publisher* httppub = static_cast<http_publisher*>(ctx);
    httppub->handle_on_file_upload_connection_lost();
}

void http_publisher::handle_on_file_upload_connection_lost() {
    LOG(common::log::info) << " refresh file upload connection " << common::log::end;
    // delete file_upload_;
    // file_upload_ = new http_connection(file_upload_uri_, evbase_, evdns_, ssl_ctx_, &http_publisher::on_file_upload_connection_lost, this);
}


namespace {

bool parse_json(const std::vector<uint8_t>& buf, Json::Value* res) {
    bool ret = false;
    if(buf.size() != 0) {
        const char* begin = reinterpret_cast<const char*>(&buf[0]);
        const char* end = begin+buf.size();
        Json::Reader reader;
        if(reader.parse(begin, end, *res, false)) {
            LOG(common::log::info) << "data parsed" << common::log::end;
            ret = true;
        } else {
            LOG(common::log::err) << "cannot parse data" << common::log::end;
        }
    } else {
        LOG(common::log::err) << "empty data" << common::log::end;
    }
    return ret;
}

void make_request_with_body(const std::string& method,
                            const std::string& path,
                            const std::string& base_uri,
                            const std::string& bearer,
                            const Json::Value& body, 
                            net::http_connection* http_connection,
                            net::http_connection::http_request_ready_cb cb, 
                            void *ctx
                            ) {

    Json::FastWriter writer;
    std::string json_str = writer.write(body);

    net::http_request* request = new net::http_request(method, path);
    request->add_header("Host", base_uri);
    request->add_header("Authorization", "Bearer "+bearer);
    request->add_header("Content-Type", "application/json");
    request->data(json_str.c_str(), json_str.size());

    http_connection->make_request(request, cb, ctx);

}

}

void http_publisher::list_folders() {

    if(state_ != initializing) {
        assert(state_ == initializing);
    }

    state_ = listing_root_folder;

    Json::Value root;
    root["path"] = "";
    root["recursive"] = false;
    root["include_media_info"] = false;
    root["include_deleted"] = false;
    root["include_has_explicit_shared_members"] = false;

    make_request_with_body("POST", "/2/files/list_folder", base_uri_, bearer_, root,
                           api_, &http_publisher::on_list_folders_complete, this
                           );

}

#define CB_TO_MEMFUN(cb_fun, mem_fun) \
    void http_publisher::cb_fun(http_request* req, http_response* res, void* ctx) { \
        http_publisher* http_pub = static_cast<http_publisher*>(ctx); \
        return http_pub->mem_fun(req, res); \
    }

CB_TO_MEMFUN(on_list_folders_complete, handle_on_list_folders_complete);

void http_publisher::handle_on_list_folders_complete(http_request* req, http_response* res) {

    if(state_ != listing_root_folder) {
        assert(state_ == listing_root_folder);
    }

    if(!validate_response(res)) {
        LOG(common::log::err) << "failed " << req->method() << " request to " << req->path() << common::log::end;
        delete req;
        delete res;
        // listing_root_dir_failed
        return;
    }

    const std::vector<uint8_t>& data = res->data();
    Json::Value json;
    bool folder_found = false;
    bool has_more = false;
    if(parse_json(data, &json)) {
        Json::Value& entries = json["entries"];
        if(entries.isArray()) {
            for(int i = 0 ; i < entries.size() ; i++) {
                Json::Value& entry = entries[i];
                if(!entry.isObject()) {
                    continue;
                }
                Json::Value& tag = entry[".tag"];
                if(!tag.isString()) {
                    continue;
                }
                if(tag.asString() == "folder") {
                    Json::Value& path_lower = entry["path_lower"];
                    if(!path_lower.isString()) {
                        continue;
                    }
                    if(path_lower.asString() == "/_seccam_") {
                        folder_found = true;
                        break;
                    }
                }
            }
            if(!folder_found) {
                Json::Value& has_more_resp = json["has_more"];
                if(has_more_resp.isBool()) {
                    has_more = has_more_resp.asBool();
                    if(has_more) {
                        Json::Value& cursor_resp = json["cursor"];
                        if(cursor_resp.isString()) {
                            std::string cursor = cursor_resp.asString();
                            list_folders_continue(cursor);
                        } else {
                            // listing_root_dir_failed
                            LOG(common::log::err) << "invalid response format cursor missing" << common::log::end;
                        }
                    } else {
                        create_app_folder();
                    }
                } else {
                    // listing_root_dir_failed
                    LOG(common::log::err) << "invalid response format has_more missing" << common::log::end;
                }
            } else {
                // folder found
                list_app_folder();
            }
        } else {
            // listing_root_dir_failed
            LOG(common::log::err) << "invalid response format entries missing" << common::log::end;
        }
    } else {
        // listing_root_dir_failed
        LOG(common::log::err) << "failed to parse response" << common::log::end;
    }

    delete req;
    delete res;
} 

void http_publisher::list_folders_continue(const std::string& cursor) {
    throw std::runtime_error("method /list_folder/continue not implemented");
}

void http_publisher::create_app_folder() {

    if(state_ != listing_root_folder) {
        assert(state_ == listing_root_folder);
    }

    state_ = creating_app_folder;

    Json::Value root;
    root["path"] = "/_seccam_";
    root["autorename"] = false;

    make_request_with_body("POST", "/2/files/create_folder_v2", base_uri_, bearer_, root,
                           api_, &http_publisher::on_create_folder_complete, this
                           );

}

CB_TO_MEMFUN(on_create_folder_complete, handle_on_create_folder_complete);

void http_publisher::handle_on_create_folder_complete(http_request* req, http_response* res) {

    if(state_ != creating_app_folder) {
        assert(state_ == creating_app_folder);
    }

    if(!validate_response(res)) {
        LOG(common::log::err) << "failed " << req->method() << " request to " << req->path() << common::log::end;
        delete res;
        delete req;
        return;
    }

    const std::vector<uint8_t>& data = res->data();

    Json::Value json;
    if(parse_json(data, &json)) {
        Json::Value& metadata = json["metadata"];
        if(metadata.isObject()) {
            Json::Value& path_lower_resp = metadata["path_lower"];
            if(path_lower_resp.isString()) {
                std::string path_lower = path_lower_resp.asString();
                if(path_lower == "/_seccam_") {
                    on_enumerate_files_complete();
                } else {
                    LOG(common::log::err) << "returned path is " << path_lower << common::log::end;
                }
            } else {
                LOG(common::log::err) << "malformed create folder response" << common::log::end;
            }
        } else {
            LOG(common::log::err) << "create folder failed" << common::log::end;
        }
    } else {
        LOG(common::log::err) << "failed to parse response" << common::log::end;
    }

    delete req;
    delete res;
}

void http_publisher::on_enumerate_files_complete() {
    LOG(common::log::info) << "connection ready" << common::log::end;
    if(state_ != listing_app_folder && state_ != listing_app_folder_continue && state_ != creating_app_folder) {
        assert(state_ == listing_app_folder ||
               state_ == listing_app_folder_continue ||
               state_ == creating_app_folder);
    }
    state_ = idle;
    on_connection_ready_(ctx_);
}

void http_publisher::pop_segment() {
    if(state_ != idle && state_ != sending_segment && state_ != sending_segment_retry) {
        assert(state_ == idle || state_ == sending_segment || state_ == sending_segment_retry);
        return;
    }

    state_ = popping_segment;

    if(segment_list_.size() > 0) {
        common::segment* seg = segment_list_.front();
        LOG(common::log::info) << "sending segment size=" << seg->size() << common::log::end;
        segment_list_.pop_front();
        send_segment(seg);
    } else {
        if (last_segment_) {
            LOG(common::log::info) << "no more segments" << common::log::end;
            state_ = terminating_video;
            on_last_request_sent_(ctx_);
        } else {
            LOG(common::log::info) << "segments list is empty - going idle" << common::log::end;
            state_ = idle;
        }
    }
}

class http_publisher::on_segment_sent_handler {
public:
    on_segment_sent_handler(http_publisher* publisher, Json::Value* root, common::segment* seg)
        : publisher_(publisher)
        , root_(root)
        , seg_(seg) {

    }
    ~on_segment_sent_handler() {

    }
private:
    on_segment_sent_handler(const on_segment_sent_handler&) = delete;
    void operator=(const on_segment_sent_handler&);
public:
    void execute(net::http_request* req, net::http_response* res) {
        publisher_->handle_on_send_segment_complete(req, res, root_, seg_);
    }
private:
    http_publisher* publisher_;
    Json::Value* root_;
    common::segment* seg_;
};

void http_publisher::send_segment(common::segment* seg) {

    if(state_ != popping_segment) {
        assert(state_ == popping_segment);
    }

    state_ = sending_segment;

    long new_size = files_size_ + seg->size();
    last_segment_ = seg->last_segment();

    if(new_size >= (1024*1024*1024)*2l) {
        // TODO: delete most recent file and then resend
    } 

    std::time_t ts = std::time(NULL);
    std::ostringstream name_str;
    name_str << ts;

    Json::Value* root_ptr = new Json::Value;
    Json::Value& root = *root_ptr;
    root["path"] = "/_seccam_/"+name_str.str();
    Json::FastWriter writer;
    std::string json_str = writer.write(root);
    json_str = json_str.substr(0, json_str.size()-1); // erase '\n'

    net::http_request* request = new net::http_request("POST", "/2/files/upload");
    request->add_header("Host", file_upload_uri_);
    request->add_header("Authorization", "Bearer "+bearer_);
    request->add_header("Dropbox-API-Arg", json_str);
    request->add_header("Content-Type", "application/octet-stream");

    request->reference_data(seg->buffer(), seg->size());

    file_upload_->make_request(request, http_publisher::on_send_segment_complete,
                               new on_segment_sent_handler(this, root_ptr, seg));

}

void http_publisher::on_send_segment_complete(http_request* req, http_response* res, void* ctx) {
    on_segment_sent_handler* hnd = static_cast<on_segment_sent_handler*>(ctx);
    hnd->execute(req, res);
    delete hnd;
}

bool http_publisher::process_upload_response(Json::Value& json) {
    bool response_ok = false;
    Json::Value& name = json["name"];
    Json::Value& last_modified = json["client_modified"];
    Json::Value& path_lower = json["path_lower"];
    Json::Value& size = json["size"];
    if(name.isString() && last_modified.isString() && path_lower.isString() && size.isNumeric()) {
        std::string timestamp_str = name.asString();
        int timestamp = std::atoi(timestamp_str.c_str());
        if(timestamp > 0) {
            api_file file = {
                timestamp,
                timestamp_str,
                path_lower.asString(),
                size.asInt(),
                last_modified.asString()
            };
            files_size_ += file.size;
            bool inserted = files_by_timestamp_.insert(std::make_pair(timestamp, file)).second;
            assert(true == inserted);
            response_ok = true;
        } else {
            LOG(common::log::err) << "invalid response filename is not a number" << common::log::end;
        }
    } else {
        LOG(common::log::err) << "invalid response" << common::log::end;
    }
    return response_ok;
}

void http_publisher::handle_on_send_segment_complete(http_request* req,
                                                     http_response* res,
                                                     Json::Value* json_arg,
                                                     common::segment* seg
                                                     ) {

    LOG(common::log::info) << "segment sent" << common::log::end;

    if(state_ != sending_segment) {
        assert(state_ == sending_segment);
    }

    if(!validate_response(res)) {
        LOG(common::log::err) << "failed " << req->method() << " request to " << req->path() << common::log::end;
        delete res;
        delete req;
        start_retry_timer(json_arg, seg, 0);
        return;
    }

    const std::vector<uint8_t>& data = res->data();
    
    bool send_new_segment = false;
    Json::Value json;
    if(parse_json(data, &json)) {
        if(process_upload_response(json)) {
            send_new_segment = true;
        } else {
            LOG(common::log::err) << "failed to process response" << common::log::end;
        }
    } else {
        LOG(common::log::err) << "failed to parse response" << common::log::end;
    }

    delete req;
    delete res;
    delete json_arg;
    delete seg;

    if(send_new_segment) {
        LOG(common::log::info) << "pop new segment" << common::log::end;
        pop_segment();
    }
}

void http_publisher::list_app_folder() {

    if(state_ != listing_root_folder) {
        assert(state_ == listing_root_folder);
    }

    state_ = listing_app_folder;

    Json::Value root;
    root["path"] = "/_seccam_";
    root["recursive"] = false;
    root["include_media_info"] = false;
    root["include_deleted"] = false;
    root["include_has_explicit_shared_members"] = false;

    make_request_with_body("POST", "/2/files/list_folder", base_uri_, bearer_, root,
                           api_, &http_publisher::on_list_app_folder_complete, this
                           );

}

CB_TO_MEMFUN(on_list_app_folder_complete, handle_on_list_app_folder_complete);

void http_publisher::list_app_folder_continue(const std::string& cursor) {

    if(state_ != listing_app_folder) {
        assert(state_ == listing_app_folder);
    }

    state_ = listing_app_folder_continue;

    Json::Value root;
    root["cursor"] = cursor;

    make_request_with_body("POST", "/2/files/list_folder/continue", base_uri_, bearer_, root,
                           api_, &http_publisher::on_list_app_folder_complete, this
                           );

}

void http_publisher::handle_on_list_app_folder_complete(http_request* req, http_response* res) {

    if(state_ != listing_app_folder && state_ != listing_app_folder_continue) {
        assert(state_ == listing_app_folder || state_ == listing_app_folder_continue);
    }

    if(!validate_response(res)) {
        LOG(common::log::err) << "failed " << req->method() << " request to " << req->path() << common::log::end;
        delete res;
        delete req;
        return;
    }

    const std::vector<uint8_t>& data = res->data();
    Json::Value json;
    if(parse_json(data, &json)) {
        Json::Value& entries  = json["entries"];
        if(entries.isArray()) {
            append_to_files(entries);
            Json::Value& has_more_resp = json["has_more"];
            if(has_more_resp.isBool()) {
                bool has_more = has_more_resp.asBool();
                if(has_more) {
                    Json::Value& cursor = json["cursor"];
                    list_app_folder_continue(cursor.asString());
                } else {
                    on_enumerate_files_complete();
                }
            } else {
                LOG(common::log::err) << "invalid response format has_more missing" << common::log::end;
            }
        } else {
            LOG(common::log::err) << "invalid response format entries missing" << common::log::end;
        }
    } else {
        LOG(common::log::err) << "failed to parse the response" << common::log::end;
    }

    delete req;
    delete res;
}

class http_publisher::retry_timer_handler {
public:
    retry_timer_handler(event* self, http_publisher* publisher, Json::Value* json_arg, common::segment* seg, int retry_cnt)
        : self_(self)
        , publisher_(publisher)
        , json_arg_(json_arg)
        , seg_(seg)
        , retry_cnt_(retry_cnt) {

    }
    ~retry_timer_handler() {
        event_free(self_);
    }
private:
    retry_timer_handler(const retry_timer_handler&);
    void operator=(const retry_timer_handler&);
public:
    void execute() {
        publisher_->handle_on_retry_expired(retry_cnt_, json_arg_, seg_);
    }
private:
    event* self_;
    http_publisher* publisher_;
    Json::Value* json_arg_;
    common::segment* seg_;
    int retry_cnt_;
};

void http_publisher::start_retry_timer(Json::Value* json_arg, common::segment* seg, int retry_count) {

    if(state_ != sending_segment && state_ != sending_segment_retry) {
        assert(state_ == sending_segment ||
               state_ == sending_segment_retry
               );
    }

    state_ = sending_segment_retry_timer;

    event* timer = static_cast<event*>(std::malloc(sizeof(event_get_struct_event_size())));
    retry_timer_handler* hnd = new retry_timer_handler(timer, this, json_arg, seg, retry_count);
    evtimer_assign(timer, evbase_, &http_publisher::on_retry_timer_expired, hnd);

    timeval timeout;
    timeout.tv_sec = initial_retry_sec_ + retry_count*initial_retry_sec_;
    timeout.tv_usec = 0;
    evtimer_add(timer, &timeout);
}

void http_publisher::on_retry_timer_expired(int, short what, void* ctx) {
    retry_timer_handler* hnd = static_cast<retry_timer_handler*>(ctx);
    hnd->execute();
    delete hnd;
}

class http_publisher::on_send_segment_retry_complete_handler {
public:

    on_send_segment_retry_complete_handler(int retry_cnt,
                                           http_publisher* publisher,
                                           Json::Value* json_arg,
                                           common::segment* seg
                                           )
        : retry_cnt_(retry_cnt)
        , publisher_(publisher)
        , json_arg_(json_arg)
        , seg_(seg) {

    }

    ~on_send_segment_retry_complete_handler() { }

private:
    on_send_segment_retry_complete_handler(const on_send_segment_retry_complete_handler&) = delete;
    void operator=(const on_send_segment_retry_complete_handler&) = delete;
public:
    void execute(http_request* req, http_response* res) {
        publisher_->handle_on_send_segment_retry_complete(req, res, retry_cnt_, json_arg_, seg_);
    }

private:
    int retry_cnt_;
    http_publisher* publisher_;
    Json::Value* json_arg_;
    common::segment* seg_;
};

void http_publisher::handle_on_retry_expired(int retry_cnt, Json::Value* json_arg, common::segment* seg) {

    if(state_ != sending_segment_retry_timer) {
        assert(sending_segment_retry_timer == state_);
    }

    state_ = sending_segment_retry;

    Json::FastWriter writer;
    std::string json_str = writer.write(*json_arg);
    json_str = json_str.substr(0, json_str.size()-1); // erase '\n'

    net::http_request* request = new net::http_request("POST", "/2/files/upload");
    request->add_header("Host", file_upload_uri_);
    request->add_header("Authorization", "Bearer "+bearer_);
    request->add_header("Dropbox-API-Arg", json_str);
    request->add_header("Content-Type", "application/octet-stream");

    request->reference_data(seg->buffer(), seg->size());

    file_upload_->make_request(request, http_publisher::on_send_segment_retry_complete,
                               new on_send_segment_retry_complete_handler(retry_cnt, this, json_arg, seg));

}

void http_publisher::on_send_segment_retry_complete(http_request* req, http_response* res, void* ctx) {
    on_send_segment_retry_complete_handler* hnd = static_cast<on_send_segment_retry_complete_handler*>(ctx);
    hnd->execute(req, res);
    delete hnd;
}

void http_publisher::handle_on_send_segment_retry_complete(http_request* req,
                                                           http_response* res,
                                                           int retry_cnt,
                                                           Json::Value* json_arg,
                                                           common::segment* seg
                                                           ) {

    if(state_ != sending_segment_retry) {
        assert(sending_segment_retry == state_);
    }

    if(!validate_response(res)) {
        LOG(common::log::err) << "failed " << req->method() << " request to " << req->path() << common::log::end;
        delete res;
        delete req;

        if(retry_cnt < max_retry_count_) {
            start_retry_timer(json_arg, seg, retry_cnt+1);
        } else {
            delete json_arg;
            delete seg;
            // TODO: terminate
            assert(NULL == "Retry expired");
        }
        return;
    }

    const std::vector<std::uint8_t>& data = res->data();

    bool send_new_segment = false;
    Json::Value json;
    if(parse_json(data, &json)) {
        if(process_upload_response(json)) {
            send_new_segment = true;
        } else {
            LOG(common::log::err) << "failed to process response" << common::log::end;
        }
    } else {
        LOG(common::log::err) << "failed to parse response" << common::log::end;
    }

    delete res;
    delete req;
    delete json_arg;
    delete seg;

    if(send_new_segment) {
        pop_segment();
    } else {
        // TODO: terminate
        assert(false);
    }

}

bool http_publisher::validate_response(http_response* res) {
    if(res == nullptr) {
        return false;
    } else if (res->response_code() != 200) {
        LOG(common::log::err) << "failed with" << res->response_code() << " " << res->response_phrase() << common::log::end;
        const std::vector<std::uint8_t>& buf = res->data();
        if(buf.size() != 0) {
            const char* begin = reinterpret_cast<const char*>(&buf[0]);
            const char* end = begin+buf.size();
            std::string data(begin, end);
            LOG(common::log::err) << "response " << data << common::log::end;
        }
        return false;
    } else {
        return true;
    }
}

void http_publisher::append_to_files(Json::Value& entries) {
    for(int i = 0 ; i < entries.size() ; i++) {
        Json::Value& entry = entries[i];
        if(!entry.isObject()) {
            LOG(common::log::info) << "skip invalid entry" << common::log::end;
            continue;
        }
        Json::Value& tag_resp = entry[".tag"];
        if(!tag_resp.isString()) {
            LOG(common::log::info) << "skip invalid entry" << common::log::end;
            continue;
        }
        std::string tag = tag_resp.asString();
        if(tag == "file") {

            std::string filename = entry["name"].asString();
            int timestamp = std::atoi(filename.c_str());
            if(0 == timestamp) {
                LOG(common::log::err) <<  "invalid file " << filename << common::log::end;
                continue;
            }

            api_file file = {
                timestamp,
                filename,
                entry["path_lower"].asString(),
                entry["size"].asInt(),
                entry["client_modified"].asString()
            };

            files_size_ += file.size;

            bool inserted = files_by_timestamp_.insert(std::make_pair(timestamp, file)).second;
            assert(true == inserted);

        } else {
            LOG(common::log::err) << "found unexpected entry " << tag << " ignoring" << common::log::end;
        }
    }
}


