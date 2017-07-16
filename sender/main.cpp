#include <unistd.h>
#include <sys/socket.h>
#include <sys/fcntl.h>

#include "logging/log.h"

#include "video/v4l_capture.h"
#include "video/h264_encoder.h"
#include "video/segmenter.h"
#include "common/segment.h"

#include "net/http_publisher.h"

#include <cassert>
#include <iostream>
#include <csignal>
#include <cstdio>
#include <sstream>
#include <thread>
#include <functional>
#include <atomic>
#include <cstring>

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
    video_capture(int vc_writer_fd)
        : capture_(nullptr)
        , encoder_(nullptr)
        , segmenter_(nullptr)
        , thread_(nullptr)
        , stop_(false)
        , vc_writer_fd_(vc_writer_fd) {

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
    // called from std::thread video_capture::thread_
    void handle_on_segment_ready(common::segment* segment) {
        LOG(common::log::info) << "writing segment" << common::log::end;
        std::uintptr_t seg_ptr = reinterpret_cast<uintptr_t>(segment);
        write(vc_writer_fd_, &seg_ptr, sizeof(std::uintptr_t));
    }

    static void on_eof(void* ctx) {
        video_capture* vid_cap = static_cast<video_capture*>(ctx);
        vid_cap->handle_on_eof();
    }

    void handle_on_eof() {
        // called on video capture thread
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
    video::v4l_capture* capture_;
    video::h264_encoder* encoder_;
    video::segmenter* segmenter_;
    std::thread* thread_;
    std::atomic<bool> stop_;
    int vc_writer_fd_;
};

class ctl_interface {
public:
    ctl_interface(event_base* ev_base, video_capture& capture)
        : ev_base_(ev_base)
        , capture_(capture) {
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
    std::string bearer = "I_MCABHFgHwAAAAAAAAAFTXna1oZPqjwpu8VI63W42xP7yRuBH_nVjiEo2YDQm4B";

    int queue_fds[2];
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, queue_fds) < 0) {
        assert(NULL == "cannot create socket pair");
    }
    
    fcntl(queue_fds[0], F_SETFL, fcntl(queue_fds[0], F_GETFL, 0) | O_NONBLOCK);
    fcntl(queue_fds[1], F_SETFL, fcntl(queue_fds[1], F_GETFL, 0) | O_NONBLOCK);

    {
        video_capture capture(queue_fds[0]);

        {

            ctl_interface ctl(evbase, capture);

            net::http_publisher publisher(base_uri, file_upload_uri, bearer, evbase, evdns, ssl_ctx, queue_fds[1],
                                          on_connection_ready, on_connection_error, on_last_request_sent, &ctl
                                         );

            

            event *sigevent = evsignal_new(evbase, SIGINT, sigint_function, &ctl);
            event_add(sigevent, NULL);

            event_base_dispatch(evbase);

            event_free(sigevent);
        }
    }

    while(true) {
        std::uintptr_t seg_ptr;
        ssize_t rc = read(queue_fds[1], &seg_ptr, sizeof(std::uintptr_t));
        if(rc == sizeof(std::uintptr_t)) {
            common::segment* seg = reinterpret_cast<common::segment*>(seg_ptr);
            delete seg;
            assert(false);
        } else {
            break;
        }
    }

    close(queue_fds[0]);
    close(queue_fds[1]);

    evdns_base_free(evdns, 0); 
    event_base_free(evbase);
    SSL_CTX_free(ssl_ctx);

    return 0;

}
