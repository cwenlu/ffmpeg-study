//
// Created by Administrator on 2017/4/19 0019.
//

#include "marge_image2video.h"
#include <libavformat/avformat.h>

static void logv(char *method_name, int ret){
    printf("%s : %d(%s)",method_name,ret,av_err2str(ret));
}

//测试解图片
//av_register_all();
//AVFrame *frame=av_frame_alloc();
//decodeJpg("C:/Users/Administrator.PC-20160506VZIM/Desktop/frame/image24.jpg",frame);
int decodeJpg(const char *file,AVFrame *frame){
    int ret;
    AVFormatContext *fmt_ctx=NULL;
    AVCodecContext *codec_ctx=NULL;
    AVCodec *codec=NULL;
    int video_index=-1;

    int test=0;

    fmt_ctx=avformat_alloc_context();
    ret=avformat_open_input(&fmt_ctx,file,NULL,NULL);
    if(ret<0){
        logv("avformat_open_input",ret);
        return ret;
    }
    ret=avformat_find_stream_info(fmt_ctx,NULL);
    if(ret){
        logv("avformat_find_stream_info",ret);
        return ret;
    }
    for(int i=0;i<fmt_ctx->nb_streams;i++){
        if(fmt_ctx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            video_index=i;
            break;
        }
    }
    if(video_index==-1){
        printf("not find video stream");
        return video_index;
    }
    codec=avcodec_find_decoder(fmt_ctx->streams[video_index]->codecpar->codec_id);
    if(!codec){
        goto end;
    }
    codec_ctx=avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx,fmt_ctx->streams[video_index]->codecpar);
    AVDictionary *param = 0;
    av_dict_set(&param, "preset", "slow", 0);
    av_dict_set(&param, "tune", "zerolatency", 0);
    ret=avcodec_open2(codec_ctx,codec,/*NULL*/&param);
    if(ret<0){
        logv("avcodec_open2",ret);
        return -1;
    }
#if test
    int y_size=codec_ctx->width*codec_ctx->height;
#endif
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data=NULL;
    pkt.size=0;
#if test
    FILE *out_file=fopen("test.yuv","wb");
#endif
    //这里一帧图片循环只会执行一次
    while /*if*/(av_read_frame(fmt_ctx,&pkt)>=0){
        if(pkt.stream_index==video_index){
            send:
            ret=avcodec_send_packet(codec_ctx,&pkt);
            if(ret<0){
                if(ret==AVERROR(EAGAIN)){
                    goto send;
                }
                break;
            }
            ret=avcodec_receive_frame(codec_ctx,frame);
            if(ret<0){
                if(ret==AVERROR(EAGAIN)){
                    goto send;
                }
                break;
            }
#if test
            //保存为yuv文件
            fwrite(frame->data[0],1,y_size,out_file);
            fwrite(frame->data[1],1,y_size/4,out_file);
            fwrite(frame->data[2],1,y_size/4,out_file);
#endif

        }

#if test
        fclose(out_file);
#endif

    }
    av_packet_unref(&pkt);
    end:
        avformat_close_input(&fmt_ctx);
        avcodec_close(codec_ctx);
    return -1;
}



void margeImage2Video(const char *in_file, const char *out_file){
    AVFormatContext *fmt_ctx=NULL;
    AVCodecContext *codec_ctx=NULL;
    AVCodec *codec=NULL;
    AVCodecParameters *codec_parameters=NULL;
    AVStream *video_st=NULL;
    AVPacket pkt;
    int ret=-1;

    av_register_all();
    ret=avformat_alloc_output_context2(&fmt_ctx,NULL,NULL,out_file);
    if(ret<0){
        logv("avformat_alloc_output_context2",ret);
        return;
    }
    ret=avio_open(&fmt_ctx->pb,out_file,AVIO_FLAG_READ_WRITE);
    if(ret<0){
        logv("avio_open",ret);
        goto end;
    }
    video_st=avformat_new_stream(fmt_ctx,NULL);
    if(!video_st){
        goto end;
    }
    codec=avcodec_find_encoder(fmt_ctx->oformat->video_codec);
    codec_ctx=avcodec_alloc_context3(codec);
    codec_ctx->width=1280;
    codec_ctx->height=720;
    codec_ctx->pix_fmt=AV_PIX_FMT_YUV420P;//AV_PIX_FMT_YUVJ420P
    codec_ctx->time_base.num=1;
    codec_ctx->time_base.den=25;
    codec_ctx->gop_size=1;

    ret=avcodec_open2(codec_ctx,codec,NULL);
    if(ret<0){
        logv("avcodec_open2",ret);
        goto end;
    }

    if(fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){
        codec_ctx->flags |=AV_CODEC_FLAG_GLOBAL_HEADER;
    }


    //<<<<<<<<<<<必须设置这个，不然编码一些容器格式会出错。（h264没问题）eg:MP4<<<<<<<<<<<<<<<<<<
    //<<<<<<<<<早起一些示例codec_ctx=video_st->codec,直接设置的，新版将一些参数设置放到了AVCodecParameters中<<<<<<
    //<<<<<<<<<<<<Could not find tag for codec none in stream #0, codec not currently supported in container<<<<<<<<<<<<<<
    codec_parameters=avcodec_parameters_alloc();
    ret=avcodec_parameters_from_context(codec_parameters,codec_ctx);
    if(ret<0){
        logv("avcodec_parameters_from_context",ret);
        goto end;
    }
    video_st->codecpar=codec_parameters;
    //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    avformat_write_header(fmt_ctx,NULL);
    av_init_packet(&pkt);
    pkt.data=NULL;
    pkt.size=0;

    AVFrame *frame=av_frame_alloc();
    char *name_header="image";
    char *name_suffix=".jpg";
    for(int i=1;i<=/*1405*/300;i++){
        char index[5];//需要记住结束符\0也要占一个位置
        sprintf(index,"%d",i);
        char *full_name=malloc(strlen(in_file)+strlen(name_header)+strlen(index)+strlen(name_suffix));
        strcpy(full_name,in_file);
        strcat(full_name,name_header);
        strcat(full_name,index);
        strcat(full_name,name_suffix);
        printf("%s\n",full_name);
        decodeJpg(full_name,frame);
        double frame_between=AV_TIME_BASE*av_q2d(codec_ctx->time_base);
        frame->pts=av_rescale_q(i*frame_between,AV_TIME_BASE_Q,video_st->time_base);
    send:
        ret=avcodec_send_frame(codec_ctx,frame);
        if(ret<0){
            if(AVERROR(EAGAIN)==ret){
                goto send;
            }
            logv("avcodec_send_frame",ret);
            goto end;
        }
        ret=avcodec_receive_packet(codec_ctx,&pkt);
        if(ret<0){
            //可能出现Resource temporarily unavailable，需要再次发送
            if(AVERROR(EAGAIN)==ret){
                goto send;
//                continue;
            }
            logv("avcodec_receive_packet",ret);
            goto end;
        }

        av_write_frame(fmt_ctx,&pkt);
        if(!full_name){
            free(full_name);
        }

    }
    av_packet_unref(&pkt);
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


}
