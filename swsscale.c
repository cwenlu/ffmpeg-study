//
// Created by Administrator on 2017/4/18 0018.
//

#include "swsscale.h"
#include <stdio.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
static void logv(char *method_name, int ret){
    printf("%s : %d(%s)",method_name,ret,av_err2str(ret));
}

void yuv_rotate_180(uint8_t *des,uint8_t *src,int width,int height)
{
    int n = 0;
    int hw = width / 2;
    int hh = height / 2;
    //copy y
    for(int j = height - 1; j >= 0; j--)
    {
        for(int i = width; i > 0; i--)
        {
            des[n++] = src[width*j + i];
        }
    }

    //copy u
    uint8_t *ptemp = src + width * height;
    for(int j = hh - 1;j >= 0; j--)
    {
        for(int i = hw; i > 0; i--)
        {
            des[n++] = ptemp[hw * j + i];
        }
    }

    //copy v
    ptemp += width * height / 4;
    for(int j = hh - 1;j >= 0; j--)
    {
        for(int i = hw; i > 0; i--)
        {
            des[n++] = ptemp[hw * j + i];
        }
    }
}

void convert(){
    FILE *in_file=fopen("akiyo_qcif.yuv","rb");
    FILE *out_file=fopen("akiyo_cif.rgb","wb");
    int src_bpp=av_get_bits_per_pixel(av_pix_fmt_desc_get(AV_PIX_FMT_YUV420P));
    int dst_bpp=av_get_bits_per_pixel(av_pix_fmt_desc_get(AV_PIX_FMT_RGB32));
    const int src_w=176,src_h=144;
    const int dst_w=352,dst_h=240;
    uint8_t *frame_buff=av_malloc(src_w*src_h*src_bpp/8);
    uint8_t *src_ptr[4],*dst_ptr[4];
    int src_linesize[4],dst_linesize[4];
    int ret=av_image_alloc(src_ptr,src_linesize,src_w,src_h,AV_PIX_FMT_YUV420P,1);
    if(ret<0){
        logv("av_image_alloc",ret);
        goto end;
    }

    ret=av_image_alloc(dst_ptr,dst_linesize,dst_w,dst_h,AV_PIX_FMT_RGB32,1);
    if(ret<0){
        logv("av_image_alloc",ret);
        goto end;
    }

//    struct SwsContext *sws_ctx=sws_getContext(src_w,src_h,AV_PIX_FMT_YUV420P,dst_w,dst_h,AV_PIX_FMT_RGB24,SWS_BICUBIC,NULL,NULL,NULL);

    struct SwsContext *sws_ctx =sws_alloc_context();
    //Show AVOption
    av_opt_show2(sws_ctx,stdout,AV_OPT_FLAG_VIDEO_PARAM,0);
    //Set Value
    av_opt_set_int(sws_ctx,"sws_flags",SWS_BICUBIC|SWS_PRINT_INFO,0);
    av_opt_set_int(sws_ctx,"srcw",src_w,0);
    av_opt_set_int(sws_ctx,"srch",src_h,0);
    av_opt_set_int(sws_ctx,"src_format",AV_PIX_FMT_YUV420P,0);
    //'0' for MPEG (Y:0-235);'1' for JPEG (Y:0-255)
    av_opt_set_int(sws_ctx,"src_range",1,0);
    av_opt_set_int(sws_ctx,"dstw",dst_w,0);
    av_opt_set_int(sws_ctx,"dsth",dst_h,0);
    av_opt_set_int(sws_ctx,"dst_format",AV_PIX_FMT_RGB32,0);
    av_opt_set_int(sws_ctx,"dst_range",1,0);
    sws_init_context(sws_ctx,NULL,NULL);

    while(1){
        if(fread(frame_buff,1,src_w*src_h*src_bpp/8,in_file)!=src_w*src_h*src_bpp/8){
            break;
        }
        //先翻转yuv180°不然后面直接转会颠倒(转成rgb24时会颠倒)
//        uint8_t *dst=av_malloc(src_w*src_h*src_bpp/8);
//        yuv_rotate_180(dst,frame_buff,src_w,src_h);
//        memcpy(frame_buff,dst,src_w*src_h*src_bpp/8);

        memcpy(src_ptr[0],frame_buff,src_w*src_h);
        memcpy(src_ptr[1],frame_buff+src_w*src_h,src_w*src_h/4);
        memcpy(src_ptr[2],frame_buff+src_w*src_h*5/4,src_w*src_h/4);

        sws_scale(sws_ctx,src_ptr,src_linesize,0,src_h,dst_ptr,dst_linesize);

//        fwrite(dst_ptr[0],1,dst_w*dst_h*4,out_file);//rgb32
        fwrite(dst_ptr[0],1,dst_w*dst_h*dst_bpp/8,out_file);//rgb32,dst_bpp/8为每个像素所占字节数
//        fwrite(dst_ptr[0],1,dst_w*dst_h*3,out_file);//rgb24
//        fwrite(dst_ptr[0],1,dst_w*dst_h*dst_bpp/8,out_file);//rgb24

    }
    end:
    if(sws_ctx){
        sws_freeContext(sws_ctx);
    }
    fclose(in_file);
    fclose(out_file);
    if(src_ptr[0]){
        av_freep(&src_ptr[0]);
    }
    if(dst_ptr[0]){
       av_freep(&dst_ptr[0]);
    }
    if(frame_buff){
        av_free(frame_buff);
    }

}