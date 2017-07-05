#include <unistd.h>

#include "video/v4l_capture.h"
#include "video/h264_encoder.h"
#include "video/segmenter.h"
#include "video/segment.h"
#include "queue.h"

#include <cassert>
#include <iostream>
#include <csignal>
#include <cstdio>
#include <sstream>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

static volatile sig_atomic_t stop;

void handle_stop(int) {
    stop = 1;
}

int count = 0;

void handle_segment_ready(video::segment* seg, void* ctx) {
    std::cout << "MAIN handle_segment_ready seg->size=" << seg->size() << std::endl;
    std::ostringstream sstr;
    sstr << "segment_" << (count++) << ".h264";
    std::FILE *fp = std::fopen(sstr.str().c_str(), "w");
    fwrite(seg->buffer(), 1, seg->size(), fp);
    std::fclose(fp);
    delete seg;
}

void handle_eof(void* ctx) {
    std::cout << "MAIN handle_eof" << std::endl;
}

int main(int,char*[]) {

    signal(SIGINT, handle_stop);

    avdevice_register_all();
    avcodec_register_all();
    av_register_all();

    common::queue queue;

    video::v4l_capture capture;
    video::h264_encoder encoder;
    video::segmenter seg(handle_segment_ready, handle_eof, &queue);

    capture.attach_sink(&encoder);
    encoder.attach_sink(&seg);
    
    while(!stop && capture.capture())
        std::cout << "frame captured" << std::endl;
    
    std::cout << "stop capture" << std::endl;
    capture.stop_capture();
    std::cout << "stop capture done" << std::endl;

}
