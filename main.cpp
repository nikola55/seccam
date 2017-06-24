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

video::v4l_capture* gcap;

void handle_stop(int) {
    gcap->stop_capture();
}

int main(int,char*[]) {

    signal(SIGINT, handle_stop);

    avdevice_register_all();
    avcodec_register_all();
    av_register_all();

    AVRational fps = { 1, 30 };
    video::v4l_capture capture(640, 480, &fps);
    video::h264_encoder encoder;

    capture.attach_sink(&encoder);
    gcap = &capture; // TODO: remove
    capture.start_capture();
    


    // AVInputFormat *input_format = av_find_input_format("v4l2");
    // assert(NULL != input_format);

    // AVFormatContext *format_context = NULL;
    // AVDictionary *options = NULL;
    // av_dict_set(&options, "framerate", "7.5", 0);
    // av_dict_set(&options, "width", "640", 0);
    // av_dict_set(&options, "height", "480", 0);

    // if(avformat_open_input(&format_context, "/dev/video0", input_format, &options) != 0) {
    //     std::cout<<"cannot open video source" << std::endl;
    //     return 1;
    // }
    // if (avformat_find_stream_info(format_context, NULL) < 0) {
    //     std::cout << "Could not find stream information" << std::endl;
    //     return 1;
    // }

    // int i = 8;

    // // avdevice_capabilities_create()

    // while(i--) {
    //     AVPacket* pkt = av_packet_alloc();
    //     if(0 != av_read_frame(format_context, pkt)) {
    //         std::cout << "cannot read frame" << std::endl;
    //     }
    //     std::cout << pkt->size << std::endl;
    //     av_packet_free(&pkt);
    // }


    // avformat_close_input(&format_context);



}
