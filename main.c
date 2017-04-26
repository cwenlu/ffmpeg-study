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
#include "pcm2aac.h"
#include "remuxing.h"
//int main() {
//
////    yuv2jpg();
////    yuv2h264();
////    decoder();
////    decoder2();
////    convert();
////    margeImage2Video("C:/Users/Administrator.PC-20160506VZIM/Desktop/frame/","C:/Users/Administrator.PC-20160506VZIM/Desktop/frame/marge.mp4");
////    pcm2aac();
////    remux("C:/Users/Administrator.PC-20160506VZIM/Desktop/kkk.mp4","C:/Users/Administrator.PC-20160506VZIM/Desktop/kkk_copy.mp4");
////    demux();
//    return 0;
//}


