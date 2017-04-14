//
// Created by Administrator on 2017/4/13 0013.
//
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include "yuv2h264.h"

static void flush_encoder(AVFormatContext *fmt_ctx,AVCodecContext *codec_ctx){
    if(!(codec_ctx->codec->capabilities & AV_CODEC_CAP_DELAY)){
        return;
    }
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data=NULL;
    pkt.size=0;
    int got_packet_ptr=0;
    int ret=-1;
    while(1){
        ret=avcodec_encode_video2 (codec_ctx, &pkt,
                               NULL, &got_packet_ptr);
        if(ret<0){
            break;
        }
        if(!got_packet_ptr){
            break;
        }
        ret=av_write_frame(fmt_ctx,&pkt);
        if(ret<0){
            break;
        }

    }
    av_packet_unref(&pkt);
}

//FILE *in_file=fopen("akiyo_qcif.yuv","rb+");
void yuv2h264(){
    static char *out_file="yuv2h264.h264";
    AVFormatContext *fmt_ctx=NULL;
    AVOutputFormat *ofmt=NULL;
    AVCodecContext *codec_ctx=NULL;
    AVStream *video_st=NULL;
    AVCodec *codec=NULL;
    AVFrame *frame=NULL;
    AVPacket pkt;
    uint8_t *frame_buff=NULL;
    int frame_size;
    int ret;
    int pkt_inited=0;

    av_register_all();
//    avformat_alloc_output_context2(&fmt_ctx,NULL,NULL,out_file);
    fmt_ctx=avformat_alloc_context();
    ofmt=av_guess_format(NULL,out_file,NULL);
    fmt_ctx->oformat=ofmt;
    ret=avio_open(&fmt_ctx->pb,out_file,AVIO_FLAG_READ_WRITE);
    if(ret<0){
        printf("avio_open : %d(%s)",ret,av_err2str(ret));
        goto end;
    }

    video_st=avformat_new_stream(fmt_ctx,NULL);

    codec=avcodec_find_encoder(ofmt->video_codec);
    if(!codec){
        printf("avcodec_find_encoder fail");
        goto end;
    }
    codec_ctx=avcodec_alloc_context3(codec);
    codec_ctx->width=176;
    codec_ctx->height=144;
    codec_ctx->pix_fmt=AV_PIX_FMT_YUV420P;
//    codec_ctx->bit_rate=400000;
    codec_ctx->time_base.num=1;
    codec_ctx->time_base.den=25;
    codec_ctx->gop_size=1;
//    codec_ctx->qmin = 10;
//    codec_ctx->qmax = 51;
    AVDictionary *param=NULL;
    av_dict_set(&param, "preset", "slow", 0);
    av_dict_set(&param, "tune", "zerolatency", 0);
    ret=avcodec_open2(codec_ctx,codec,/*&param*/NULL);
    if(ret<0){
        printf("avcodec_open2 : %d(%s)",ret,av_err2str(ret));
        goto end;
    }
    frame=av_frame_alloc();
    frame->format=codec_ctx->pix_fmt;
    frame->width=codec_ctx->width;
    frame->height=codec_ctx->height;
    frame->pts=0;
    frame_size=av_image_get_buffer_size(codec_ctx->pix_fmt,codec_ctx->width,codec_ctx->height,1);
    frame_buff=av_malloc(frame_size);
    av_image_fill_arrays(frame->data,frame->linesize,frame_buff,codec_ctx->pix_fmt,codec_ctx->width,codec_ctx->height,1);

    avformat_write_header(fmt_ctx,NULL);
    av_init_packet(&pkt);
    pkt.data=NULL;
    pkt.size=0;
    pkt_inited=1;

    int y_size=codec_ctx->width*codec_ctx->height;
    double pre_frame_s=av_q2d(codec_ctx->time_base);

    FILE *in_file=fopen("akiyo_qcif.yuv","rb+");
    int i=0;
    while(!feof(in_file)){

        fread(frame_buff,1,frame_size,in_file);
        frame->data[0]=frame_buff;              //y
        frame->data[1]=frame_buff+y_size;       //u
        frame->data[2]=frame_buff+y_size*5/4;   //v

        double pre_frame_us=AV_TIME_BASE*pre_frame_s;
        frame->pts=i*pre_frame_us;
        i++;


        //这种方式avcodec_receive_packet总是报错
//        ret=avcodec_send_frame(codec_ctx,frame);
//        if(ret<0){
//            printf("avcodec_send_frame : %d(%s)",ret,av_err2str(ret));
//            goto end;
//        }
//        ret=avcodec_receive_packet(codec_ctx,&pkt);
//        if(ret<0){
//            printf("avcodec_receive_packet : %d(%s)",ret,av_err2str(ret));
//            goto end;
//        }
        int got_packet_ptr;
        avcodec_encode_video2(codec_ctx,&pkt,frame,&got_packet_ptr);
        av_write_frame(fmt_ctx,&pkt);
    }
    flush_encoder(fmt_ctx,codec_ctx);
    av_write_trailer(fmt_ctx);

    end:
    if(fmt_ctx){
        avformat_free_context(fmt_ctx);
    }
    if(codec_ctx){
        avcodec_close(codec_ctx);
        avcodec_free_context(&codec_ctx);
    }
    if(frame){
        av_frame_free(&frame);
    }
    if(frame_buff){
        av_free(frame_buff);
    }
    if(pkt_inited){
        av_packet_unref(&pkt);
    }



}
