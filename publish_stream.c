//
// Created by Administrator on 2017/6/2 0002.
//

#include "publish_stream.h"
#include <libavformat/avformat.h>
#include <libavutil/time.h>

static void logv(char *method_name, int ret) {
    printf("%s : %d(%s)", method_name, ret, av_err2str(ret));
}

//rtmp推流
void rtmp_publish(){
    const char *in_filename  = "input.mp4";//"sample.flv"
    const char *out_filename = "rtmp://localhost/oflaDemo/stream1441847046975";
    AVFormatContext *ifmt_ctx=NULL,*ofmt_ctx=NULL;
    AVPacket pkt;
    int ret=-1;
    int in_video_index=-1;
    int frame_index=0;

    av_register_all();
    avformat_network_init();
    //input
    ret=avformat_open_input(&ifmt_ctx,in_filename,NULL,NULL);
    if(ret<0){
        logv("avformat_open_input",ret);
        goto end;
    }
    ret=avformat_find_stream_info(ifmt_ctx,NULL);
    if(ret<0){
        logv("avformat_find_stream_info",ret);
        goto end;
    }

    //output
    avformat_alloc_output_context2(&ofmt_ctx,NULL,"flv",out_filename);//rtmp
//    avformat_alloc_output_context2(&ofmt_ctx,NULL,"mpegts",out_filename);//udp
    for (int i = 0; i <ifmt_ctx->nb_streams ; ++i) {
        AVStream *in_stream=ifmt_ctx->streams[i];
        if(in_stream->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            in_video_index=i;
        }
        AVStream *out_stream=avformat_new_stream(ofmt_ctx,in_stream->codec->codec);
        ret=avcodec_copy_context(out_stream->codec,in_stream->codec);
        if(ret<0){
            logv("avcodec_copy_context",ret);
            goto end;
        }
        out_stream->codec->codec_tag=0;
        if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
    }
    if(!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)){
        ret=avio_open(&ofmt_ctx->pb,out_filename,AVIO_FLAG_WRITE);
        if(ret<0){
            logv("avio_open",ret);
            goto end;
        }
    }
    ret=avformat_write_header(ofmt_ctx,NULL);
    if(ret<0){
        logv("avformat_write_header",ret);
        goto end;
    }

    uint64_t start_time=av_gettime();
    while(1){
        AVStream *in_stream=NULL,*out_stream=NULL;
        ret=av_read_frame(ifmt_ctx,&pkt);
        if(ret<0){
            break;
        }
        in_stream=ifmt_ctx->streams[pkt.stream_index];
        out_stream=ofmt_ctx->streams[pkt.stream_index];
        if(pkt.pts==AV_NOPTS_VALUE){
            AVRational target_time_base=in_stream->time_base;
            int64_t frame_distance=(int64_t)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
            pkt.pts=av_rescale_q(frame_index*frame_distance,AV_TIME_BASE_Q,target_time_base);
            pkt.dts=pkt.pts;
            pkt.duration=av_rescale_q(frame_distance,AV_TIME_BASE_Q,target_time_base);
        }
        //延时发送
        if(pkt.stream_index==in_video_index){
            //stream转化为微妙时间基
            uint64_t pts_time=av_rescale_q(pkt.pts,in_stream->time_base,AV_TIME_BASE_Q);
            uint64_t now_time=av_gettime()-start_time;
            if(pts_time>now_time){
                av_usleep(pts_time-now_time);
            }
        }

        //convert pts/dts
        pkt.pts=av_rescale_q_rnd(pkt.pts,in_stream->time_base,out_stream->time_base,AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        pkt.dts=av_rescale_q_rnd(pkt.dts,in_stream->time_base,out_stream->time_base,AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        pkt.duration=av_rescale_q(pkt.duration,in_stream->time_base,out_stream->time_base);
        pkt.pos-1;
        if(pkt.stream_index==in_video_index){
            printf("Send %8d video frames to output URL\n",frame_index);
            frame_index++;
        }
        ret=av_interleaved_write_frame(ofmt_ctx,&pkt);
        if(ret<0){
            logv("av_interleaved_write_frame",ret);
            goto end;
        }
        av_packet_unref(&pkt);
    }

    av_write_trailer(ofmt_ctx);

    end:
    avformat_network_deinit();
    avformat_close_input(&ifmt_ctx);
    if(ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)){
        avio_close(ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);
}