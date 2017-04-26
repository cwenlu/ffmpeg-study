//
// Created by Administrator on 2017/4/24 0024.
//

#include "pcm2aac.h"
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
static void logv(char *method_name, int ret){
    printf("%s : %d(%s)",method_name,ret,av_err2str(ret));
}
/*有问题，暂时不知道为啥*/
void pcm2aac(){
    char *out_file="pcm2aac.aac";
    FILE *in_file=fopen("tdjm.pcm","rb");
    AVFormatContext *fmt_ctx=NULL;
    AVOutputFormat *ofmt=NULL;
    AVStream *audio_st=NULL;
    AVCodecContext *codec_ctx=NULL;
    AVCodecParameters *codec_params=NULL;
    AVCodec *codec=NULL;
    AVFrame *frame=NULL;
    AVPacket pkt;
    uint8_t *frame_buff=NULL;
    int ret=-1;

    av_register_all();
    fmt_ctx=avformat_alloc_context();
    ofmt=av_guess_format(NULL,out_file,NULL);
    fmt_ctx->oformat=ofmt;
    if((ret=avio_open(&fmt_ctx->pb,out_file,AVIO_FLAG_READ_WRITE))<0){
        logv("avio_open",ret);
        return;
    }
    audio_st=avformat_new_stream(fmt_ctx,NULL);
    if(audio_st==NULL){
        printf("avformat_new_stream fail");
        return;
    }
    codec_ctx=avcodec_alloc_context3(NULL);
    codec_ctx->codec_id=ofmt->audio_codec;
    codec_ctx->codec_type=AVMEDIA_TYPE_AUDIO;
    codec_ctx->sample_fmt=AV_SAMPLE_FMT_FLTP;
    codec_ctx->sample_rate=44100;
    codec_ctx->channel_layout=AV_CH_LAYOUT_STEREO;
    codec_ctx->channels=av_get_channel_layout_nb_channels(codec_ctx->channel_layout);
    codec_ctx->bit_rate=64000;
    codec_ctx->codec_tag=0;

    codec_params=avcodec_parameters_alloc();
    avcodec_parameters_from_context(codec_params,codec_ctx);
    audio_st=codec_params;

    codec=avcodec_find_encoder(codec_ctx->codec_id);
    if(!codec){
        printf("avcodec_find_encoder fail");
        goto end;
    }
    ret=avcodec_open2(codec_ctx,codec,NULL);
    if(ret<0){
        logv("avcodec_open2",ret);
        goto end;
    }
    frame=av_frame_alloc();
    frame->nb_samples=codec_ctx->frame_size;
    frame->format=codec_ctx->sample_fmt;
    int size=av_samples_get_buffer_size(NULL,codec_params->channels,codec_ctx->frame_size,codec_params->format,1);
    if(size<0){
        logv("av_samples_get_buffer_size",size);
        goto end;
    }
    frame_buff=av_malloc(size);
    avcodec_fill_audio_frame(frame,codec_params->channels,codec_params->format,frame_buff,size,1);

    if(ofmt->flags & AVFMT_GLOBALHEADER){
        audio_st->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    avformat_write_header(fmt_ctx,NULL);

    av_init_packet(&pkt);
    pkt.data=NULL;
    pkt.size=0;

    int i=0;
    while(fread(frame_buff,1,size,in_file)>0 && !feof(in_file)){
        frame->data[0]=frame_buff;
        frame->pts=i;
        i++;

        ret=avcodec_send_frame(codec_ctx,frame);
        if(ret<0){
            if(AVERROR(EAGAIN)==ret){
                continue;
            }
            logv("avcodec_send_frame",ret);
            break;
        }
        ret=avcodec_receive_packet(codec_ctx,&pkt);
        if(ret<0){
            if(AVERROR(EAGAIN)==ret){
                continue;
            }
            logv("avcodec_receive_packet",ret);
            break;
        }

        av_write_frame(fmt_ctx,&pkt);
    }
//    flush encoder
    av_write_trailer(fmt_ctx);
    end:
    if(codec_params){
        avcodec_parameters_free(&codec_params);
    }
    avcodec_close(codec_ctx);
    avio_close(fmt_ctx->pb);
    avformat_free_context(fmt_ctx);
    if(frame_buff){
        av_free(frame_buff);
    }
    if(frame){
        av_frame_free(&frame);
    }
    if(in_file){
        fclose(in_file);
    }

}