//
// Created by Administrator on 2017/4/13 0013.
//
#include "yuv2jpg.h"
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
void yuv2jpg(){
    AVFormatContext *ofmt_ctx=NULL;
    AVStream *video_st=NULL;
    AVCodecContext *codec_ctx=NULL;
    AVCodec *codec=NULL;
    AVFrame *frame=NULL;
    AVPacket pkt;
    const char *out_file="yuv2jpg.jpg";
    int ret=0;
    int frame_size;
    uint8_t *frame_buff=NULL;
    int pkt_inited=0;
    av_register_all();
    avformat_alloc_output_context2(&ofmt_ctx,NULL,NULL,out_file);
    ret=avio_open(&ofmt_ctx->pb,out_file,AVIO_FLAG_READ_WRITE);
    if(ret<0){
        printf("avio_open : %d(%s)",ret,av_err2str(ret));
        goto end;
    }
    video_st=avformat_new_stream(ofmt_ctx,NULL);
    if(!video_st){
        printf("avformat_new_stream fail");
        goto end;
    }


    codec_ctx = avcodec_alloc_context3(NULL);
    codec_ctx->codec_id = ofmt_ctx->oformat->video_codec;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    codec_ctx->width=176;
    codec_ctx->height=144;

//    奇怪这里这样写居然没起作用，ffmpeg提示没有设置time_base
//    codec_ctx->time_base=(AVRational){1/25};
    codec_ctx->time_base.num=1;
    codec_ctx->time_base.den=25;
    codec=avcodec_find_encoder(codec_ctx->codec_id);

    if(!codec){
        printf("avcodec_find_encoder fail");
        goto end;
    }
    codec_ctx->codec=codec;
    ret=avcodec_open2(codec_ctx,codec,NULL);
    if(ret<0){
        printf("avcodec_open2 : %d(%s)",ret,av_err2str(ret));
        goto end;
    }

    frame=av_frame_alloc();
    //<<<<<must set<<<<<<
    frame->format=codec_ctx->pix_fmt;
    frame->width=codec_ctx->width;
    frame->height=codec_ctx->height;
    //必须指定pts
    frame->pts=1;
    //>>>>>>>>>>>>>>>>>

    frame_size=av_image_get_buffer_size(codec_ctx->pix_fmt,codec_ctx->width,codec_ctx->height,1);
    frame_buff=av_malloc(frame_size);
    av_image_fill_arrays(frame->data,frame->linesize,frame_buff,codec_ctx->pix_fmt,codec_ctx->width,codec_ctx->height,1);

    avformat_write_header(ofmt_ctx,NULL);
    av_init_packet(&pkt);
    pkt_inited=1;

    //<<<<读取yuv数据<<<<
    FILE *in_file=fopen("akiyo_qcif.yuv","rb+");
    fread(frame_buff,1,frame_size,in_file);
    //>>>>>>>>>>>>>>>>>>>

    int y_size=codec_ctx->width*codec_ctx->height;
    frame->data[0]=frame_buff;              //y
    frame->data[1]=frame_buff+y_size;       //u
    frame->data[2]=frame_buff+y_size*5/4;   //v

//    avcodec_encode_video2()过时
    ret=avcodec_send_frame(codec_ctx,frame);
    if(ret<0){
        printf("avcodec_send_frame : %d(%s)",ret,av_err2str(ret));
        goto end;
    }
    ret=avcodec_receive_packet(codec_ctx,&pkt);
    if(ret<0){
        printf("avcodec_receive_packet : %d(%s)",ret,av_err2str(ret));
        goto end;
    }
    pkt.stream_index=video_st->index;
    av_write_frame(ofmt_ctx,&pkt);
    av_write_trailer(ofmt_ctx);

    end:
    avio_close(ofmt_ctx->pb);
    if(ofmt_ctx){
        avformat_free_context(ofmt_ctx);
    }
    if(codec_ctx){
        avcodec_close(codec_ctx);
        avcodec_free_context(&codec_ctx);
    }
    if(frame_buff){
        av_free(frame_buff);
    }
    if(pkt_inited){
        av_packet_unref(&pkt);
    }



}
