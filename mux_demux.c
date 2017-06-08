//
// Created by Administrator on 2017/6/1 0001.
//

#include "mux_demux.h"
#include <libavformat/avformat.h>
static void logv(char *method_name, int ret) {
    printf("%s : %d(%s)", method_name, ret, av_err2str(ret));
}
//复用音视频为mp4
void mux(){
    const char *in_video="input.h264";
    const char *in_audio="huoyuanjia.mp3";
//    const char *in_audio="test.aac";
    const char *out_av="mux_self.mp4";

    int ret=0;
    AVFormatContext *ifmt_ctx_v=NULL,*ifmt_ctx_a=NULL,*ofmt_ctx=NULL;
    int video_index=0,audio_index=0;//输入的流索引
    int vsi=0,asi=0;//输出的流索引
    AVPacket pkt;

    av_register_all();
    //input
    ret=avformat_open_input(&ifmt_ctx_v,in_video,NULL,NULL);
    if(ret<0){
        logv("avformat_open_input v",ret);
        goto end;
    }
    ret=avformat_open_input(&ifmt_ctx_a,in_audio,NULL,NULL);
    if(ret<0){
        logv("avformat_open_input a",ret);
        goto end;
    }
    ret=avformat_find_stream_info(ifmt_ctx_v,NULL);
    if(ret<0){
        logv("avformat_find_stream_info v",ret);
        goto end;
    }
    ret=avformat_find_stream_info(ifmt_ctx_a,NULL);
    if(ret<0){
        logv("avformat_find_stream_info a",ret);
        goto end;
    }

    //output
    ret=avformat_alloc_output_context2(&ofmt_ctx,NULL,NULL,out_av);
    if(ret<0){
        logv("avformat_alloc_output_context2",ret);
        goto end;
    }
    //[copy video info]
    ret=av_find_best_stream(ifmt_ctx_v,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
    if(ret<0){
        logv("av_find_best_stream video",ret);
        goto end;
    }
    video_index=ret;
    AVStream *in_stream=ifmt_ctx_v->streams[ret];
    AVStream *out_stream=avformat_new_stream(ofmt_ctx,in_stream->codec->codec);
    vsi=out_stream->index;
    avcodec_copy_context(out_stream->codec,in_stream->codec);
    if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){
        out_stream->codec->flags | CODEC_FLAG_GLOBAL_HEADER;
    }
    //[copy audio info]
    ret=av_find_best_stream(ifmt_ctx_a,AVMEDIA_TYPE_AUDIO,-1,-1,NULL,0);
    if(ret<0){
        logv("av_find_best_stream audio",ret);
        goto end;
    }
    audio_index=ret;
    in_stream=ifmt_ctx_a->streams[ret];
    out_stream=avformat_new_stream(ofmt_ctx,in_stream->codec->codec);
    asi=out_stream->index;
    avcodec_copy_context(out_stream->codec,in_stream->codec);
    if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){
        out_stream->codec->flags | CODEC_FLAG_GLOBAL_HEADER;
    }

    if(!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)){
        ret=avio_open(&ofmt_ctx->pb,out_av,AVIO_FLAG_WRITE);
        if(ret<0){
            logv("avio_open",ret);
            goto end;
        }
    }
    if((ret=avformat_write_header(ofmt_ctx,NULL))<0){
        logv("avformat_write_header",ret);
        goto end;
    }

    int64_t ts_a=0,ts_b=0;
    int frame_index=0;
    while (1){
        int stream_index=0;
        AVStream *in_stream, *out_stream;

        //视频时间小于等于音频时间
        if(av_compare_ts(ts_a,ifmt_ctx_v->streams[video_index]->time_base,ts_b,ifmt_ctx_a->streams[audio_index]->time_base)<=0){
            stream_index=vsi;
            if(av_read_frame(ifmt_ctx_v,&pkt)>=0){
                do{
                    if(pkt.stream_index==video_index){
                        in_stream=ifmt_ctx_v->streams[pkt.stream_index];
                        out_stream=ofmt_ctx->streams[stream_index];
                        pkt.stream_index=stream_index;//这里不设置留到最外层convert设置也可以
                        if(pkt.pts==AV_NOPTS_VALUE){
                            //目标输出时间基
                            AVRational target_time_base=in_stream->time_base;
                            //2帧之间时间差(us)内部时间
                            int64_t frame_distance=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
                            pkt.pts=av_rescale_q(frame_index*frame_distance,AV_TIME_BASE_Q,target_time_base);
                            pkt.dts=pkt.pts;
                            pkt.duration=av_rescale_q(frame_distance,AV_TIME_BASE_Q,target_time_base);
                            frame_index++;
                        }
                        ts_a=pkt.pts;
                        break;
                    }
                }while (av_read_frame(ifmt_ctx_v,&pkt)>=0);
            } else{
                break;
            }
        } else{
            if(av_read_frame(ifmt_ctx_a,&pkt)>=0){
                stream_index=asi;
                do{
                    if(pkt.stream_index==audio_index){
                        in_stream=ifmt_ctx_a->streams[pkt.stream_index];
                        out_stream=ofmt_ctx->streams[stream_index];
                        pkt.stream_index=stream_index;
                        if(pkt.pts == AV_NOPTS_VALUE){
                            //目标输出时间基
                            AVRational target_time_base=in_stream->time_base;
                            //2帧之间时间差(us)内部时间
                            int64_t frame_distance=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
                            pkt.pts=av_rescale_q(frame_index*frame_distance,AV_TIME_BASE_Q,target_time_base);
                            pkt.dts=pkt.dts;
                            pkt.duration=av_rescale_q(frame_distance,AV_TIME_BASE_Q,target_time_base);
                            frame_index++;
                        }
                        ts_b=pkt.pts;
                        break;
                    }
                }while (av_read_frame(ifmt_ctx_a,&pkt)>=0);
            }else{
                break;
            }
        }

        //Convert PTS/DTS,将输入转换为输出时间
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index=stream_index;
        printf("Write 1 Packet. size:%5d\tpts:%lld\tstream index:%d\n",pkt.size,pkt.pts,pkt.stream_index);
        //write
        if ((ret=av_interleaved_write_frame(ofmt_ctx, &pkt)) < 0) {
            logv("av_interleaved_write_frame",ret);
            break;
        }
        av_packet_unref(&pkt);
    }
    av_write_trailer(ofmt_ctx);
end:
    avformat_close_input(&ifmt_ctx_v);
    avformat_close_input(&ifmt_ctx_a);
    if(ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)){
        avio_close(ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);

}







//分离音视频流
void demuxer(){
#define USE_H264BSF 0
//    这个弄出的h264不能放，aac可以
//    const char *in_file="cuc_ieschool.ts";
//    const char *out_audio="cuc_ieschool.aac";
//    const char *out_video="cuc_ieschool.h264";

//    这个h264可以，aac不可以
    const char *in_file="input.mp4";
    const char *out_audio="input.aac";
    const char *out_video="input.h264";

    AVFormatContext *ifmt_ctx=NULL,*ofmt_ctx_v=NULL,*ofmt_ctx_a=NULL;
    AVPacket pkt;
    int ret=-1;
    int video_index=-1,audio_index=-1;

    av_register_all();
    //根据输入创建输出
    ret=avformat_open_input(&ifmt_ctx,in_file,NULL,NULL);
    if(ret<0){
        logv("avformat_open_input",ret);
        goto end;
    }
    ret=avformat_find_stream_info(ifmt_ctx,NULL);
    if(ret<0){
        logv("avformat_find_stream_info",ret);
        goto end;
    }
    avformat_alloc_output_context2(&ofmt_ctx_v,NULL,NULL,out_video);
    avformat_alloc_output_context2(&ofmt_ctx_a,NULL,NULL,out_audio);
    for(int i=0;i<ifmt_ctx->nb_streams;i++){
        AVFormatContext *ofmt_ctx=NULL;

        AVStream *in_stream=ifmt_ctx->streams[i];
        AVStream *out_stream=NULL;
        if(in_stream->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            ofmt_ctx=ofmt_ctx_v;
            video_index=i;
        }else if(in_stream->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            ofmt_ctx=ofmt_ctx_a;
            audio_index=i;
        }else{
            continue;
        }
        out_stream=avformat_new_stream(ofmt_ctx,in_stream->codec->codec);
        if(!out_stream){
            printf("avformat_new_stream fail");
            goto end;
        }
        out_stream->codec->codec_tag=0;
        if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }

    }

    //打开流
    if(!(ofmt_ctx_v->oformat->flags & AVFMT_NOFILE)){
        ret=avio_open(&ofmt_ctx_v->pb,out_video,AVIO_FLAG_WRITE);
        if(ret<0){
            logv("avio_open v",ret);
            goto end;
        }
    }
    if(!(ofmt_ctx_a->oformat->flags & AVFMT_NOFILE)){
        ret=avio_open(&ofmt_ctx_a->pb,out_audio,AVIO_FLAG_WRITE);
        if(ret<0){
            logv("avio_open a",ret);
            goto end;
        }
    }
    ret=avformat_write_header(ofmt_ctx_v,NULL);
    if(ret<0){
        logv("avformat_write_header v",ret);
        goto end;
    }
    ret=avformat_write_header(ofmt_ctx_a,NULL);
    if(ret<0){
        logv("avformat_write_header a",ret);
        goto end;
    }
#if USE_H264BSF
    AVBitStreamFilterContext *h264_bsfc=av_bitstream_filter_init("h264_mp4toannexb");
#endif
    while(1){
        AVFormatContext *ofmt_ctx=NULL;
        AVStream *in_stream=NULL,*out_stream=NULL;
        ret=av_read_frame(ifmt_ctx,&pkt);
        if(ret<0){
            logv("av_read_frame",ret);
            break;
        }
        in_stream=ifmt_ctx->streams[pkt.stream_index];
        if(pkt.stream_index==video_index){
            ofmt_ctx=ofmt_ctx_v;
#if USE_H264BSF
            av_bitstream_filter_filter(h264_bsfc,in_stream->codec,NULL,&pkt.data,&pkt.size,pkt.data,pkt.size,0);
#endif
        }else if(pkt.stream_index==audio_index){
            ofmt_ctx=ofmt_ctx_a;
        }else{
            continue;
        }
        out_stream=ofmt_ctx->streams[0];

        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index=0;

        ret=av_interleaved_write_frame(ofmt_ctx,&pkt);
        if(ret<0){
            logv("av_interleaved_write_frame",ret);
            break;
        }

        av_packet_unref(&pkt);
    }
#if USE_H264BSF
    av_bitstream_filter_close(h264_bsfc);
#endif
    av_write_trailer(ofmt_ctx_v);
    av_write_trailer(ofmt_ctx_a);

    end:
    avformat_close_input(&ifmt_ctx);
    if(ofmt_ctx_v && !(ofmt_ctx_v->oformat->flags & AVFMT_NOFILE)){
        avio_close(ofmt_ctx_v->pb);
    }
    if(ofmt_ctx_a && !(ofmt_ctx_a->oformat->flags & AVFMT_NOFILE)){
        avio_close(ofmt_ctx_a->pb);
    }
    avformat_free_context(ofmt_ctx_v);
    avformat_free_context(ofmt_ctx_a);

}
/**
 * 直接保存pkt，手动添加adts，勉强可参考
 * http://blog.chinaunix.net/xmlrpc.php?r=blog/article&uid=24922718&id=3692670
 * @return
 */
int demuxer2(){
    const char *in_file="input.mp4";//"cuc_ieschool.ts",这里面提取的h264播放不了

    static int audioindex = -1;
    static int videoindex = -1;
    static int isaaccodec = -1;

    av_register_all();
    FILE *f = NULL;
    FILE *g = NULL;
    f = fopen("audio.aac","wb");
    g = fopen("video.h264","wb");
    if(!f || !g)
    {
        printf("open write file errorn");
        return 0;
    }
    AVFormatContext *fmtctx = NULL;

    AVPacket audiopack;
    if(avformat_open_input(&fmtctx,/*argv[1]*/in_file,NULL,NULL) < 0)
    {
        printf("open fmtctx errorn");
        return 0;
    }

    if(avformat_find_stream_info(fmtctx,NULL) < 0)
    {
        printf("find stream info n");
        return 0;
    }
    int streamnum = fmtctx->nb_streams;
    printf("stream num is %d\n",streamnum);
    int i=0;
    for(;i<streamnum;i++)
    {
        if(fmtctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && audioindex == -1)
        {
            audioindex = i;
        }
        else if(fmtctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && videoindex == -1)
        {
            videoindex = i;
        }
    }
    printf("audioindex is %d\n",audioindex);

    printf("videoindex is %d\n",videoindex);

    AVCodecContext *codecctx = fmtctx->streams[videoindex]->codec;
    AVCodec *decode = avcodec_find_decoder(codecctx->codec_id);

    AVCodecContext *audioCodecCtx = fmtctx->streams[audioindex]->codec;
    AVCodec *audiodecode = avcodec_find_decoder(audioCodecCtx->codec_id);
    if(audiodecode->id == AV_CODEC_ID_AAC)
    {
        isaaccodec = 1;
    }

    if(avcodec_open2(codecctx,decode,NULL) < 0)
    {
        return -1;
    }
    if(avcodec_open2(audioCodecCtx,audiodecode,NULL) < 0)
    {
        return -1;
    }
    printf("extradata size is %d\n",audioCodecCtx->extradata_size);

    AVBitStreamFilterContext* bsfc = av_bitstream_filter_init("h264_mp4toannexb");
    AVBitStreamFilterContext* aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
    if(!bsfc || !aacbsfc)
    {
        return 0;
    }
    AVFrame picture;
    while(!(av_read_frame(fmtctx,&audiopack)))
    {
        if(audiopack.stream_index == audioindex)
        {
            if(isaaccodec == 1 && audioCodecCtx->extradata!=NULL)
            {
                char bits[7] = {0};
                int sample_index = 0 , channel = 0;
                char temp = 0;
                int length = 7 + audiopack.size;
                sample_index = (audioCodecCtx->extradata[0] & 0x07) << 1;
                temp = (audioCodecCtx->extradata[1]&0x80);
                switch(audioCodecCtx->sample_rate)
                {
                    case 44100:
                    {
                        sample_index = 0x7;
                    }break;
                    default:
                    {
                        sample_index = sample_index + (temp>>7);
                    }break;
                }
                channel = ((audioCodecCtx->extradata[1] - temp) & 0xff) >> 3;
                bits[0] = 0xff;
                bits[1] = 0xf1;
                bits[2] = 0x40 | (sample_index<<2) | (channel>>2);
                bits[3] = ((channel&0x3)<<6) | (length >>11);
                bits[4] = (length>>3) & 0xff;
                bits[5] = ((length<<5) & 0xff) | 0x1f;
                bits[6] = 0xfc;

                fwrite(bits,1,7,f);
            }
            fwrite(audiopack.data,1,audiopack.size,f);
            printf("audio pts is %f\n",audiopack.pts*av_q2d(fmtctx->streams[audioindex]->time_base));
        }
        else if(audiopack.stream_index == videoindex){
            AVPacket pkt = audiopack;

            int a = av_bitstream_filter_filter(bsfc, codecctx, NULL, &pkt.data, &pkt.size, audiopack.data, audiopack.size, audiopack.flags & AV_PKT_FLAG_KEY);
            if(a > 0)
            {
                audiopack = pkt;
            }
            fwrite(audiopack.data,1,audiopack.size,g);
            int gotfinished = 0;

            printf("video pts is %f, %d\n",picture.pkt_dts * av_q2d(fmtctx->streams[videoindex]->codec->time_base),picture.pts );
        }
        av_free_packet(&audiopack);
    }
    av_bitstream_filter_close(bsfc);
    fclose(f);
}