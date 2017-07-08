#include "http_publisher.h"

#include "common/async_segment_queue.h"
#include "common/segment.h"

#include "http_connection.h"
#include "http_request.h"
#include "http_response.h"

#include <json/json.h>

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cassert>
#include <chrono>

using net::http_publisher;

http_publisher::http_publisher(
    const std::string& base_uri,
    const std::string& file_upload_uri,
    const std::string& bearer,
    event_base* evbase,
    evdns_base* evdns,
    SSL_CTX* ssl_ctx,
    common::async_segment_queue& queue,
    connection_event_cb on_connection_ready, 
    connection_event_cb on_connection_error, 
    void* ctx
    )
    : base_uri_(base_uri)
    , file_upload_uri_(file_upload_uri)
    , bearer_(bearer)
    , evbase_(evbase)
    , evdns_(evdns)
    , ssl_ctx_(ssl_ctx)
    , queue_(queue)
    , on_connection_ready_(on_connection_ready)
    , on_connection_error_(on_connection_error)
    , ctx_(ctx)
    , api_(nullptr)
    , file_upload_(nullptr)
    , files_size_(0)
    , exit_pending_(false) {


    try {
        api_ = new http_connection(base_uri, evbase_, evdns_, ssl_ctx_, &http_publisher::on_connection_lost, this);
    } catch (const std::runtime_error& err) {
        std::cout << " HTTP PUBLISHER http_publisher::http_publisher " << err.what() << std::endl;
        return;
    }
    
    try {
        file_upload_ = new http_connection(file_upload_uri, evbase_, evdns_, ssl_ctx_, &http_publisher::on_connection_lost, this);
    } catch (const std::runtime_error& err) {
        delete api_;
        std::cout << " HTTP PUBLISHER http_publisher::http_publisher " << err.what() << std::endl;
        return;
    }

    list_folders();

}

http_publisher::~http_publisher() {
    delete file_upload_;
    delete api_;
}

void http_publisher::on_connection_lost(void* ctx) {
    http_publisher* httppub = static_cast<http_publisher*>(ctx);
    httppub->handle_on_connection_lost();
}

void http_publisher::handle_on_connection_lost() {
    // TODO: invent retry mechanism
}

namespace {

bool parse_json(const std::vector<uint8_t>& buf, Json::Value* res) {
    bool ret = false;
    if(buf.size() != 0) {
        const char* begin = reinterpret_cast<const char*>(&buf[0]);
        const char* end = begin+buf.size();
        Json::Reader reader;
        if(reader.parse(begin, end, *res, false)) {
            std::cout << "parse_json: data parsed" << std::endl;
            ret = true;
        } else {
            std::cout << "parse_json: cannot parse data" << std::endl;
        }
    } else {
        std::cout << "parse_json: empty data" << std::endl;
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
    if(res == nullptr) {
        std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_folders_complete: " 
                  << "failed " << req->path() << " method " << req->method() << std::endl;
        delete req;
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
                            std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_folders_complete: "
                                      << "invalid response format cursor missing" << std::endl;
                        }
                    } else {
                        create_app_folder();
                    }
                } else {
                    std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_folders_complete: "
                              << "invalid response format has_more missing" << std::endl;
                    
                }
            } else {
                // folder found
                list_app_folder();
            }
        } else {
            std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_folders_complete: "
                      << "invalid response format entries missing" << std::endl;
        }
    } else {
        std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_folders_complete: failed to parse response " << std::endl;
    }

    delete req;
    delete res;
} 

void http_publisher::list_folders_continue(const std::string& cursor) {
    throw std::runtime_error("method /list_folder/continue not implemented");
}

void http_publisher::create_app_folder() {

    std::cout << "HTTP PUBLISHER http_publisher::on_folder_not_found" << std::endl;

    Json::Value root;
    root["path"] = "/_seccam_";
    root["autorename"] = false;

    make_request_with_body("POST", "/2/files/create_folder_v2", base_uri_, bearer_, root,
                           api_, &http_publisher::on_create_folder_complete, this
                           );

}

CB_TO_MEMFUN(on_create_folder_complete, handle_on_create_folder_complete);

void http_publisher::handle_on_create_folder_complete(http_request* req, http_response* res) {
    if(res == nullptr) {
        std::cout << "HTTP PUBLISHER http_publisher::handle_on_create_folder_complete: " 
                  << "failed " << req->path() << " method " << req->method() << std::endl;
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
                    std::cout << "HTTP PUBLISHER http_publisher::handle_on_create_folder_complete: " 
                              << "error! returned path is " << path_lower << std::endl;
                }
            } else {
                std::cout << "HTTP PUBLISHER http_publisher::handle_on_create_folder_complete: " 
                          << "malformed create folder response" << std::endl;
            }
        } else {
            std::cout << "HTTP PUBLISHER http_publisher::handle_on_create_folder_complete: " 
                      << "create folder failed" << std::endl;
        }
    } else {
        std::cout << "HTTP PUBLISHER http_publisher::handle_on_create_folder_complete: " 
                  << "failed to parse the response" << std::endl;
    }

    delete req;
    delete res;
}

void http_publisher::on_enumerate_files_complete() {
    std::cout << "HTTP PUBLISHER http_publisher::on_enumerate_files_complete" << std::endl;
    on_connection_ready_(ctx_);
    pop_segment();
}

void http_publisher::pop_segment() {
    common::segment* seg = nullptr;
    while(true) {
        seg = queue_.pop(std::chrono::milliseconds(6000));
        if(nullptr != seg) {
            break;
        } else if (exit_pending_) {
            break;
        }
    }
    if(nullptr != seg) {
        send_segment(seg);
    }
}

static void free_segment(void* ctx) {
    std::cout << "HTTP PUBLISHER delete segment" << std::endl;
    common::segment* seg = static_cast<common::segment*>(ctx);
    delete seg;
}

void http_publisher::send_segment(common::segment* seg) {

    long new_size = files_size_ + seg->size();

    if(new_size >= (1024*1024*1024)*2l) {
        // TODO: delete most recent file and then resend
    } 

    std::time_t ts = std::time(NULL);
    std::ostringstream name_str;
    name_str << ts;

    Json::Value root;
    root["path"] = "/_seccam_/"+name_str.str();
    Json::FastWriter writer;
    std::string json_str = writer.write(root);
    json_str = json_str.substr(0, json_str.size()-1); // erase '\n'

    net::http_request* request = new net::http_request("POST", "/2/files/upload");
    request->add_header("Host", file_upload_uri_);
    request->add_header("Authorization", "Bearer "+bearer_);
    request->add_header("Dropbox-API-Arg", json_str);
    request->add_header("Content-Type", "application/octet-stream");

    request->reference_data(seg->buffer(), seg->size(), &free_segment, seg);

    file_upload_->make_request(request, http_publisher::on_send_segment_complete, this);

}

CB_TO_MEMFUN(on_send_segment_complete, handle_on_send_segment_complete);

void http_publisher::handle_on_send_segment_complete(http_request* req, http_response* res) {
    std::cout << "HTTP PUBLISHER http_publisher::handle_on_send_segment_complete" << std::endl;
    if(!res) {
        std::cout << "HTTP PUBLISHER http_publisher::handle_on_send_segment_complete: " 
                  << "failed " << req->path() << " method " << req->method() << std::endl;
        delete req;
        return;
    }

    const std::vector<uint8_t>& data = res->data();
    
    bool send_new_segment = false;
    Json::Value json;
    if(parse_json(data, &json)) {
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
                send_new_segment = true;
            } else {
                std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_app_folder_complete: "
                      << "invalid response filename is not a number" << std::endl;
            }
        } else {
            std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_app_folder_complete: "
                      << "invalid response " << std::endl;
        }
    } else {
        std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_app_folder_complete: " 
                  << "failed to parse the response" << std::endl;
    }


    

    delete req;
    delete res;

    if(send_new_segment) {
        pop_segment();
    }
}

void http_publisher::list_app_folder() {
    std::cout << "HTTP PUBLISHER http_publisher::on_folder_found" << std::endl;

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

void http_publisher::append_to_files(Json::Value& entries) {
    for(int i = 0 ; i < entries.size() ; i++) {
        Json::Value& entry = entries[i];
        if(!entry.isObject()) {
            std::cout << "HTTP PUBLISHER http_publisher::append_to_files: skip invalid entry" << std::endl;
            continue;
        }
        Json::Value& tag_resp = entry[".tag"];
        if(!tag_resp.isString()) {
            std::cout << "HTTP PUBLISHER http_publisher::append_to_files: skip invalid entry" << std::endl;
            continue;
        }
        std::string tag = tag_resp.asString();
        if(tag == "file") {

            std::string filename = entry["name"].asString();
            int timestamp = std::atoi(filename.c_str());
            if(0 == timestamp) {
                std::cout << "HTTP PUBLISHER http_publisher::append_to_files: "
                            << " invalid file " << filename << std::endl;
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
            std::cout << "HTTP PUBLISHER http_publisher::append_to_files: "
                        << "found unexpected entry " << tag << " ignoring" << std::endl;
        }
    }
}

void http_publisher::handle_on_list_app_folder_complete(http_request* req, http_response* res) {
    std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_app_folder_complete" << std::endl;
    if(!res) {
        std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_app_folder_complete: " 
                  << "failed " << req->path() << " method " << req->method() << std::endl;
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
                std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_app_folder_complete: "
                      << "invalid response format has_more missing" << std::endl;
            }
        } else {
            std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_app_folder_complete: "
                      << "invalid response format entries missing" << std::endl;
        }
    } else {
        std::cout << "HTTP PUBLISHER http_publisher::handle_on_list_app_folder_complete: " 
                  << "failed to parse the response" << std::endl;
    }

    delete req;
    delete res;
}

void http_publisher::list_app_folder_continue(const std::string& cursor) {
    std::cout << "HTTP PUBLISHER http_publisher::on_folder_found" << std::endl;

    Json::Value root;
    root["cursor"] = cursor;

    make_request_with_body("POST", "/2/files/list_folder/continue", base_uri_, bearer_, root,
                           api_, &http_publisher::on_list_app_folder_complete, this
                           );

}

