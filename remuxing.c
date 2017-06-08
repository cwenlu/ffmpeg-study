//
// Created by Administrator on 2017/4/24 0024.
//

#include "remuxing.h"
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>


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
 * 视频封装转换，从一个容器格式到另一个容器格式
 * 但是实际测试是输入MP4输出MP4，flv，mov可以，如果输出eg: avi，则会有问题
 * 只是转封装，对于avi需要使用 h264_mp4toannexb将startcode，sps，pps等信息添加到pkt
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
    AVBitStreamFilterContext *h264bsfc=av_bitstream_filter_init("h264_mp4toannexb");
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
        if(in_stream->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        av_bitstream_filter_filter(h264bsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }
        av_packet_unref(&pkt);
    }
    av_bitstream_filter_close(h264bsfc);

    av_write_trailer(ofmt_ctx);
    end:

    avformat_close_input(&ifmt_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);


}


//转码封装mp4
int transcode4video()
{
    const char* SRC_FILE = "cuc_ieschool.ts";
//    const char* SRC_FILE = "cuc_ieschool.flv";
    const char* OUT_FILE = "outfile.h264";
    const char* OUT_FMT_FILE = "outfmtfile.avi";
//    const char* OUT_FMT_FILE = "outfmtfile.mp4";
    av_register_all();

    AVFormatContext* pFormat = NULL;
    if (avformat_open_input(&pFormat, SRC_FILE, NULL, NULL) < 0)
    {
        return 0;
    }
    AVCodecContext* video_dec_ctx = NULL;
    AVCodec* video_dec = NULL;
    if (avformat_find_stream_info(pFormat, NULL) < 0)
    {
        return 0;
    }
    av_dump_format(pFormat, 0, SRC_FILE, 0);
    video_dec_ctx = pFormat->streams[0]->codec;
    video_dec = avcodec_find_decoder(video_dec_ctx->codec_id);
    if (avcodec_open2(video_dec_ctx, video_dec, NULL) < 0)
    {
        return 0;
    }

    AVFormatContext* pOFormat = NULL;
    AVOutputFormat* ofmt = NULL;
    if (avformat_alloc_output_context2(&pOFormat, NULL, NULL, OUT_FILE) < 0)
    {
        return 0;
    }
    ofmt = pOFormat->oformat;
    if (avio_open(&(pOFormat->pb), OUT_FILE, AVIO_FLAG_READ_WRITE) < 0)
    {
        return 0;
    }
    AVCodecContext *video_enc_ctx = NULL;
    AVCodec *video_enc = NULL;
    video_enc = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVStream *video_st = avformat_new_stream(pOFormat, video_enc);
    if (!video_st)
        return 0;
    video_enc_ctx = video_st->codec;
    video_enc_ctx->width = video_dec_ctx->width;
    video_enc_ctx->height = video_dec_ctx->height;
    video_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    video_enc_ctx->time_base.num = 1;
    video_enc_ctx->time_base.den = 25;
    video_enc_ctx->bit_rate = video_dec_ctx->bit_rate;
    video_enc_ctx->gop_size = 250;
    video_enc_ctx->max_b_frames = 10;
    //H264
    //pCodecCtx->me_range = 16;
    //pCodecCtx->max_qdiff = 4;
    video_enc_ctx->qmin = 10;
    video_enc_ctx->qmax = 51;
    if (avcodec_open2(video_enc_ctx, video_enc, NULL) < 0)
    {
        printf("编码器打开失败！\n");
        return 0;
    }
    printf("Output264video Information====================\n");
    av_dump_format(pOFormat, 0, OUT_FILE, 1);
    printf("Output264video Information====================\n");

    //mp4 file
    AVFormatContext* pMp4Format = NULL;
    AVOutputFormat* pMp4OFormat = NULL;
    if (avformat_alloc_output_context2(&pMp4Format, NULL, NULL, OUT_FMT_FILE) < 0)
    {
        return 0;
    }
    pMp4OFormat = pMp4Format->oformat;
    if (avio_open(&(pMp4Format->pb), OUT_FMT_FILE, AVIO_FLAG_READ_WRITE) < 0)
    {
        return 0;
    }

    for (int i = 0; i < pFormat->nb_streams; i++) {
        AVStream *in_stream = pFormat->streams[i];
        AVStream *out_stream = avformat_new_stream(pMp4Format, in_stream->codec->codec);
        if (!out_stream) {
            return 0;
        }
        int ret = 0;
        ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
            return 0;
        }
        out_stream->codec->codec_tag = 0;
        if (pMp4Format->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }


    av_dump_format(pMp4Format, 0, OUT_FMT_FILE, 1);

    if (avformat_write_header(pMp4Format, NULL) < 0)
    {
        return 0;
    }


    ////



    av_opt_set(video_enc_ctx->priv_data, "preset", "superfast", 0);
    av_opt_set(video_enc_ctx->priv_data, "tune", "zerolatency", 0);
    avformat_write_header(pOFormat, NULL);
    AVPacket pkt;
    av_init_packet(&pkt);
    AVFrame *pFrame = av_frame_alloc();
    int ts = 0;
    while (1)
    {
        if (av_read_frame(pFormat, &pkt) < 0)
        {
            avio_close(pOFormat->pb);
            av_write_trailer(pMp4Format);
            avio_close(pMp4Format->pb);
            av_packet_unref(&pkt);
            return 0;
        }
        if (pkt.stream_index == 0)
        {

            int got_picture = 0, ret = 0;
            ret = avcodec_decode_video2(video_dec_ctx, pFrame, &got_picture, &pkt);
            if (ret < 0)
            {
                av_packet_unref(&pkt);
                return 0;
            }
            pFrame->pts = pFrame->pkt_pts;//ts++;
            if (got_picture)
            {
                AVPacket tmppkt;
                av_init_packet(&tmppkt);
                int size = video_enc_ctx->width*video_enc_ctx->height * 3 / 2;
                char* buf = malloc(size);
                memset(buf, 0, size);
                tmppkt.data = (uint8_t*)buf;
                tmppkt.size = size;
                ret = avcodec_encode_video2(video_enc_ctx, &tmppkt, pFrame, &got_picture);
                if (ret < 0)
                {
                    avio_close(pOFormat->pb);
                    av_packet_unref(&pkt);
                    return 0;
                }
                if (got_picture)
                {
                    //ret = av_interleaved_write_frame(pOFormat, tmppkt);
                    AVStream *in_stream = pFormat->streams[pkt.stream_index];
                    AVStream *out_stream = pMp4Format->streams[pkt.stream_index];

                    tmppkt.pts = av_rescale_q_rnd(tmppkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
                    tmppkt.dts = av_rescale_q_rnd(tmppkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
                    tmppkt.duration = av_rescale_q(tmppkt.duration, in_stream->time_base, out_stream->time_base);
                    tmppkt.pos = -1;
                    ret = av_interleaved_write_frame(pMp4Format, &tmppkt);
                    if (ret < 0)
                        return 0;
                    av_packet_unref(&tmppkt);
                    free(buf);

                }
            }
            //avcodec_free_frame(&pFrame);
        }
        else if (pkt.stream_index == 1)
        {
            AVStream *in_stream = pFormat->streams[pkt.stream_index];
            AVStream *out_stream = pMp4Format->streams[pkt.stream_index];

            pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
            pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
            pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
            pkt.pos = -1;
            if (av_interleaved_write_frame(pMp4Format, &pkt) < 0)
                return 0;
        }
    }
    av_frame_free(&pFrame);
    return 0;
}





/**
 * 分离出h264和mp3，直接保存pkt方式
 * （输入源内部流分别是h264，mp3）
 * 这种方式有局限。如果内部是aac直接保存不能播放，没有adts，
 * 还有这个flv测试分出来的h264播放不了，换了一个文件分出来的h264又可以播放（不知道为啥）
 */
void demux(){
    AVFormatContext *fmt_ctx=NULL;
    AVPacket pkt;
    int8_t audio_index=-1,video_index=-1;
    FILE *out_video=fopen("cuc_ieschool.h264","wb");
    FILE *out_audio=fopen("cuc_ieschool.mp3","wb");

    av_register_all();
    if(avformat_open_input(&fmt_ctx,"cuc_ieschool.flv",NULL,NULL)<0){
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
