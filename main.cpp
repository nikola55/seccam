#include <unistd.h>
#include <sys/socket.h>

#include "logging/log.h"

#include "video/v4l_capture.h"
#include "video/h264_encoder.h"
#include "video/segmenter.h"
#include "common/segment.h"
#include "common/async_segment_queue.h"

#include "net/http_publisher.h"

#include <cassert>
#include <iostream>
#include <csignal>
#include <cstdio>
#include <sstream>
#include <thread>
#include <functional>
#include <atomic>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

#include <openssl/ssl.h>

#include <event2/event.h>
#include <event2/dns.h>

class video_capture {
public:
    video_capture(common::async_segment_queue& queue) 
        : queue_(queue)
        , capture_(nullptr)
        , encoder_(nullptr)
        , segmenter_(nullptr)
        , thread_(nullptr)
        , stop_(false) {

    }
    
    ~video_capture() {

    }

private:
    video_capture(const video_capture&) = delete;
    void operator=(const video_capture&) = delete;
private:
    static void on_segment_ready(common::segment* segment, void* ctx) {
        video_capture* vid_cap = static_cast<video_capture*>(ctx);
        vid_cap->handle_on_segment_ready(segment);
    }

    void handle_on_segment_ready(common::segment* segment) {
        queue_.push(segment);
    }

    static void on_eof(void* ctx) {
        video_capture* vid_cap = static_cast<video_capture*>(ctx);
        vid_cap->handle_on_eof();
    }

    void handle_on_eof() {
        // assert(false);
    }
public:
    void start_capture() {
        assert(nullptr == thread_);
        thread_ = new std::thread(&video_capture::run, this);
    }
    void stop_capture() {
        assert(nullptr != thread_);
        stop_ = true;
    }
    void join() {
        thread_->join();
        delete thread_;
        thread_ = nullptr;
    }
private:
    void run() {
        LOG(common::log::info) << "Video subsystem started" << common::log::end;
        capture_ = new video::v4l_capture;
        encoder_ = new video::h264_encoder;
        segmenter_ = new video::segmenter(&video_capture::on_segment_ready, &video_capture::on_eof, this);

        capture_->attach_sink(encoder_);
        encoder_->attach_sink(segmenter_);
        
        while(true) {
            if(stop_) {
                capture_->stop_capture();
                break;
            }
            if(!capture_->capture()) {
                break;
            }
        }

        delete segmenter_;
        delete encoder_;
        delete capture_;
    }
private:
    common::async_segment_queue& queue_;
    video::v4l_capture* capture_;
    video::h264_encoder* encoder_;
    video::segmenter* segmenter_;
    std::thread* thread_;
    std::atomic<bool> stop_;
};

class ctl_interface {
public:
    ctl_interface(event_base* ev_base, video_capture& capture, common::async_segment_queue& queue)
        : ev_base_(ev_base)
        , capture_(capture)
        , queue_(queue) {

    }
    ~ctl_interface() {} 
private:
    ctl_interface(const ctl_interface&) = delete;
    void operator=(const ctl_interface&) = delete;
public:
    void stop_capture() {
        capture_.stop_capture();
    }
    void start_capture() {
        capture_.start_capture();
    }
    void shutdown() {
        capture_.join();
        event_base_loopexit(ev_base_, NULL);
    }
private:
    event_base* const ev_base_;
    video_capture& capture_;
    common::async_segment_queue& queue_;
};

void on_connection_ready(void* ctx) {
    LOG(common::log::info) << "Starting video subsystem" << common::log::end;
    ctl_interface* ctl = static_cast<ctl_interface*>(ctx);
    ctl->start_capture();
}

void on_connection_error(void* ctx) {
    // TODO: 
}

void on_last_request_sent(void* ctx) {
    LOG(common::log::info) << "Shutdown video subsystem" << common::log::end;
    ctl_interface* ctl = static_cast<ctl_interface*>(ctx);
    ctl->shutdown();
}

void sigint_function(evutil_socket_t, short, void *ctx) {
    LOG(common::log::info) << "Stoping video subsystem" << common::log::end;
    ctl_interface* ctl = static_cast<ctl_interface*>(ctx);
    ctl->stop_capture();
}

int main(int argc,char* argv[]) {

    signal(SIGPIPE, SIG_IGN);

    avdevice_register_all();
    avcodec_register_all();
    av_register_all();

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
	SSL_library_init();
	ERR_load_crypto_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
#endif

    SSL_CTX* ssl_ctx = SSL_CTX_new(SSLv23_method());
    event_base* evbase = event_base_new();
    evdns_base* evdns = evdns_base_new(evbase, EVDNS_BASE_DISABLE_WHEN_INACTIVE);
    evdns_base_nameserver_ip_add(evdns, "8.8.8.8");

    std::string base_uri = "api.dropboxapi.com";
    std::string file_upload_uri = "content.dropboxapi.com";
    std::string bearer = "###";
    
    {
        common::async_segment_queue queue;
        video_capture capture(queue);

        {

            ctl_interface ctl(evbase, capture, queue);

            net::http_publisher publisher(base_uri, file_upload_uri, bearer, evbase, evdns, ssl_ctx, queue, 
                                          on_connection_ready, on_connection_error, on_last_request_sent, &ctl
                                         );

            

            event *sigevent = evsignal_new(evbase, SIGINT, sigint_function, &ctl);
            event_add(sigevent, NULL);

            event_base_dispatch(evbase);

            event_free(sigevent);
        }
    }

    evdns_base_free(evdns, 0); 
    event_base_free(evbase);
    SSL_CTX_free(ssl_ctx);

    return 0;

}
