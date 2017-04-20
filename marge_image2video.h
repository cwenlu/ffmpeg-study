//
// Created by Administrator on 2017/4/19 0019.
//

#ifndef FFMPEG_STUDY_MARGE_IMAGE2VIDEO_H
#define FFMPEG_STUDY_MARGE_IMAGE2VIDEO_H

#include <libavutil/frame.h>

int decodeJpg(const char *file, AVFrame *frame);

void margeImage2Video(const char *in_file, const char *out_file);


#endif //FFMPEG_STUDY_MARGE_IMAGE2VIDEO_H
