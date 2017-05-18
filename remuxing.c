//
// Created by Administrator on 2017/4/24 0024.
//

#include "remuxing.h"
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>


static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

//    printf("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
//           tag,
//           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
//           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
//           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
//           pkt->stream_index);
}

/**
 * 重采样视频，从一个容器格式到另一个容器格式
 * 但是实际测试是输入MP4输出MP4，如果输出eg: avi，则会有问题，暂没深究
 * @param in_file
 * @param out_file
 */
void remux(char *in_file,char *out_file){
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret=-1;
    av_register_all();

    if ((ret = avformat_open_input(&ifmt_ctx, in_file, 0, 0)) < 0) {
        fprintf(stderr, "Could not open input file '%s'", in_file);
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, in_file, 0);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_file);
    if (!ofmt_ctx) {
        fprintf(stderr, "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    ofmt = ofmt_ctx->oformat;

    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);

        if (!out_stream) {
            fprintf(stderr, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
            goto end;
        }
        out_stream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    av_dump_format(ofmt_ctx, 0, out_file, 1);

    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_file, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'", out_file);
            goto end;
        }
    }

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        goto end;
    }

    while (1) {
        AVStream *in_stream, *out_stream;

        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;

        in_stream  = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];

        log_packet(ifmt_ctx, &pkt, "in");

        /* copy packet */
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        log_packet(ofmt_ctx, &pkt, "out");

        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }
        av_packet_unref(&pkt);
    }

    av_write_trailer(ofmt_ctx);
    end:

    avformat_close_input(&ifmt_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);


}

/**
 * 分离出h264和aac
 * aac不能播放因为没有adts
 */
void demux(){
    AVFormatContext *fmt_ctx=NULL;
    AVPacket pkt;
    int8_t audio_index=-1,video_index=-1;
    FILE *out_video=fopen("input.h264","wb");
    FILE *out_audio=fopen("input.aac","wb");

    av_register_all();
    if(avformat_open_input(&fmt_ctx,"input.mp4",NULL,NULL)<0){
        printf("not open");
        return;
    }
    if(avformat_find_stream_info(fmt_ctx,NULL)<0){
        printf("find stream info fail");
        goto end;
    }
    for(int i=0;i<fmt_ctx->nb_streams;i++){
        if(fmt_ctx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            video_index=i;
        }else if(fmt_ctx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            audio_index=i;
        }
    }
    AVBitStreamFilterContext *h264bsfc=av_bitstream_filter_init("h264_mp4toannexb");
    while(av_read_frame(fmt_ctx,&pkt)>=0){
        if(pkt.stream_index==video_index){
            av_bitstream_filter_filter(h264bsfc, fmt_ctx->streams[video_index]->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
            fwrite(pkt.data,1,pkt.size,out_video);
        }else if(pkt.stream_index==audio_index){

            fwrite(pkt.data,1,pkt.size,out_audio);
        }
        av_packet_unref(&pkt);
    }
    av_bitstream_filter_close(h264bsfc);
    end:
    avformat_close_input(&fmt_ctx);
    fclose(out_video);
    fclose(out_audio);
}
