#include <unistd.h>

#include <cassert>
#include <iostream>
#include <csignal>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

#include "video/v4l_capture.h"
#include "video/h264_encoder.h"

static volatile sig_atomic_t stop;

void handle_stop(int) {
    stop = 1;
}

int main(int,char*[]) {

    signal(SIGINT, handle_stop);

    avdevice_register_all();
    avcodec_register_all();
    av_register_all();

    video::v4l_capture capture;
    video::h264_encoder encoder;

    capture.attach_sink(&encoder);
    
    while(!stop && capture.capture())
        std::cout << "frame captured" << std::endl;
    
    capture.stop_capture();

}
