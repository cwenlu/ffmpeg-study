#include <stdio.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>

#include "yuv2jpg.h"
#include "yuv2h264.h"
#include "decoder.h"
#include "swsscale.h"
#include "marge_image2video.h"
int main() {

//    yuv2jpg();
//    yuv2h264();
//    decoder();
//    decoder2();
//    convert();

    margeImage2Video("C:/Users/Administrator.PC-20160506VZIM/Desktop/frame/","C:/Users/Administrator.PC-20160506VZIM/Desktop/frame/marge.mp4");

    return 0;
}