//
// Created by Administrator on 2017/4/14 0014.
//

#include "decoder.h"
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

static void logv(char *method_name, int ret) {
    printf("%s : %d(%s)", method_name, ret, av_err2str(ret));
}

static void flush_decoder(AVCodecContext *codec_ctx, AVFrame *frame, AVPacket pkt) {
    int ret = -1;
    while (1) {
        ret = avcodec_send_packet(codec_ctx, NULL);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }
            break;
        }
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }
            break;
        }
        printf("flush decoder %d", frame->pkt_size);
        //这里就不保存yuv了，演示flush decode
    }

}

void decoder() {
    FILE *out_file = fopen("decoder_input.yuv", "wb");
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec = NULL;
    AVFrame *frame = NULL;

    int ret = -1;

    av_register_all();
    ret = avformat_open_input(&fmt_ctx, "input.mp4", NULL, NULL);
    if (ret < 0) {
        logv("avformat_open_input", ret);
        goto end;
    }
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        logv("avformat_find_stream_info", ret);
        goto end;
    }
    int video_index = -1;
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_index = i;
            break;
        }
    }
    if (video_index == -1) {
        printf("Didn't find a video stream");
        goto end;
    }
//    过时 deprecated
//    codec_ctx=fmt_ctx->streams[video_index]->codec;
    codec = avcodec_find_decoder(fmt_ctx->streams[video_index]->codecpar->codec_id);
    if (!codec) {
        logv("avcodec_find_decoder", ret);
        goto end;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_index]->codecpar);
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        logv("avcodec_open2", ret);
        goto end;
    }
    frame = av_frame_alloc();
    int y_size = codec_ctx->width * codec_ctx->height;
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == video_index) {
            send:
            ret = avcodec_send_packet(codec_ctx, &pkt);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN)) {
                    goto send;
                }
                break;
            }
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN)) {
                    goto send;
                }
                break;
            }

            //保存为yuv文件
            fwrite(frame->data[0], 1, y_size, out_file);
            fwrite(frame->data[1], 1, y_size / 4, out_file);
            fwrite(frame->data[2], 1, y_size / 4, out_file);

        }
    }
    printf("%d===", frame->linesize[0]);
    flush_decoder(codec_ctx, frame, pkt);
    av_packet_unref(&pkt);
    fclose(out_file);
    end:
    if (fmt_ctx) {
        //内部调用了avformat_free_context(fmt_ctx);
        avformat_close_input(&fmt_ctx);
    }
    if (codec_ctx) {
        avcodec_close(codec_ctx);
    }


}
/**
 * 只使用libavcodec
 */
void decoder2() {
    FILE *out_file = fopen("decoder_input2.yuv", "wb");
    FILE *in_file = fopen("yuv2h264.h264", "rb");
    const int in_buff_size = 4096;
    uint8_t in_buff[in_buff_size + AV_INPUT_BUFFER_PADDING_SIZE];
    memset(in_buff, 0, in_buff_size + AV_INPUT_BUFFER_PADDING_SIZE);
    int ret = -1;
    AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodecParserContext *codec_parser_ctx = NULL;
    AVFrame *frame = NULL;
    AVPacket pkt;

    avcodec_register_all();
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        printf("avcodec_find_decoder fail");
        return;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    codec_parser_ctx = av_parser_init(AV_CODEC_ID_H264);
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        logv("avcodec_open2", ret);
        return;
    }

    frame = av_frame_alloc();
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    int read_size = 0;
    while ((read_size = fread(in_buff, 1, in_buff_size, in_file)) > 0) {
        uint8_t *cur_ptr = in_buff;
        while (read_size > 0) {
            int len = av_parser_parse2(codec_parser_ctx, codec_ctx, &pkt.data, &pkt.size, cur_ptr, read_size,
                                       AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);
            cur_ptr += len;
            read_size -= len;
            if (pkt.size == 0) {
                continue;
            }
            //decode
            send:
            ret = avcodec_send_packet(codec_ctx, &pkt);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN)) {
                    goto send;
                }
                logv("avcodec_send_packet", ret);
                goto end;
            }
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN)) {
                    goto send;
                }
                logv("avcodec_receive_frame", ret);
                goto end;
            }
            //这样保存都是有问题的，linesize不一定等于width
//            int y_size=frame->width*frame->height;
//            fwrite(frame->data[0],1,y_size,out_file);
//            fwrite(frame->data[1],1,y_size/4,out_file);
//            fwrite(frame->data[2],1,y_size/4,out_file);
//            printf("%d,%d",frame->width,frame->height);


            for (int i = 0; i < frame->height; i++) {
                fwrite(frame->data[0] + frame->linesize[0] * i, 1, frame->width, out_file);
            }
            for (int i = 0; i < frame->height / 2; i++) {
                fwrite(frame->data[1] + frame->linesize[1] * i, 1, frame->width / 2, out_file);
            }
            for (int i = 0; i < frame->height / 2; i++) {
                fwrite(frame->data[2] + frame->linesize[2] * i, 1, frame->width / 2, out_file);
            }

        }

    }

    flush_decoder(codec_ctx, frame, pkt);
    end:
    fclose(in_file);
    fclose(out_file);
    av_parser_close(codec_parser_ctx);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);

}
/**
 *  解码音频并演示了重采样
 */
void decoder_audio() {
    FILE *out_pcm = fopen("out_pcm.pcm", "wb");
    FILE *out_convert_pcm=fopen("out_convert_pcm.pcm","wb");
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec = NULL;
    AVStream *audio_stream = NULL;
    AVFrame *frame = NULL;
    AVPacket pkt;
    SwrContext *swr_ctx=NULL;

    int ret = -1;
    av_register_all();
    if (avformat_open_input(&fmt_ctx, "input.mp4", NULL, NULL) < 0) {
        printf("Couldn't open file");
        return;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        printf("find stream info fail");
        return;
    }
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (ret < 0) {
        if (ret == AVERROR_DECODER_NOT_FOUND) {
            printf("找到流但没有找到解码器");
        } else if (ret == AVERROR_STREAM_NOT_FOUND) {
            printf("没有找到流");
        }
        return;
    }
    audio_stream = fmt_ctx->streams[ret];
    codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, audio_stream->codecpar);
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        logv("avcodec_open2", ret);
        goto end;
    }
    frame = av_frame_alloc();
    //<<<<<<<<<<<<<
    //swr_alloc_set_opts  另外一种方式
    swr_ctx=swr_alloc();
    //av_opt_set_channel_layout 可以这个
    av_opt_set_int(swr_ctx,"in_channel_layout",audio_stream->codecpar->channel_layout,0);
    av_opt_set_int(swr_ctx,"out_channel_layout",AV_CH_LAYOUT_STEREO,0);
    av_opt_set_int(swr_ctx,"in_sample_rate",audio_stream->codecpar->sample_rate,0);
    int dst_sample_rate = 48000;
    av_opt_set_int(swr_ctx, "out_sample_rate", dst_sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx,"in_sample_fmt",audio_stream->codecpar->format,0);
    av_opt_set_sample_fmt(swr_ctx,"out_sample_fmt",AV_SAMPLE_FMT_FLT,0);
    swr_init(swr_ctx);
    int64_t delay_nb_sample=swr_get_delay(swr_ctx,audio_stream->codecpar->sample_rate);
    int dst_nb_channels=av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    //>>>>>>>>>>>>>>>>

    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index != audio_stream->index) {
            continue;
        }
//        http://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html
        ret = avcodec_send_packet(codec_ctx, &pkt);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            logv("avcodec_send_packet", ret);
            goto end;
        }
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret < 0 && ret != AVERROR_EOF) {
            logv("avcodec_receive_frame", ret);
            goto end;
        }


//        这里强制改为1个通道数，我们下面只保存一个通道的数据
        int buff_size = av_samples_get_buffer_size(NULL, /*frame->channels*/1, frame->nb_samples, frame->format, 1);
//        int buff_size = frame->nb_samples * av_get_bytes_per_sample(frame->format);
        fwrite(frame->extended_data[0], 1, buff_size, out_pcm);

        //convert 重采样
        int dst_nb_samples=av_rescale_rnd(delay_nb_sample+frame->nb_samples, dst_sample_rate,frame->sample_rate , AV_ROUND_UP);
        uint8_t *output;
        av_samples_alloc(&output, NULL, dst_nb_channels, dst_sample_rate,
                         AV_SAMPLE_FMT_FLT, 1);
        dst_nb_samples = swr_convert(swr_ctx, &output, dst_nb_samples,
                                  frame->extended_data, frame->nb_samples);

        buff_size=av_samples_get_buffer_size(NULL,dst_nb_channels,dst_nb_samples,AV_SAMPLE_FMT_FLT,1);
        fwrite(output,1,buff_size,out_convert_pcm);
        av_freep(&output);

        av_packet_unref(&pkt);
    }

    end:
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    if (frame) {
        av_frame_free(&frame);
    }
    if(swr_ctx){
        swr_free(&swr_ctx);
    }
    fclose(out_pcm);
    fclose(out_convert_pcm);

}