
#include "logging/log.h"

#include <cassert>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

#include "video/h264_decoder.h"
#include "video/h264_parser.h"
#include "video/file_data_source.h"

int main(int argc,char* argv[]) {

    avdevice_register_all();
    avcodec_register_all();
    av_register_all();

    av_log_set_level(AV_LOG_VERBOSE);

    video::h264_decoder decoder;
    video::h264_parser parser;
    video::file_data_source src("/home/nikola/Downloads/1500231472");

    decoder.attach_source(&parser);
    parser.attach_source(&src);

    while(decoder.read_frame()) ;


    return 0;

}
