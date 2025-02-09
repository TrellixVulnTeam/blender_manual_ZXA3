/*
 * FFmpeg main
 * Copyright (c) 2000-2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* needed for usleep() */
#define _XOPEN_SOURCE 600

#include "config.h"
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavformat/framehook.h"
#include "libavcodec/opt.h"
#include "libavcodec/audioconvert.h"
#include "libavutil/fifo.h"
#include "libavutil/avstring.h"
#include "libavformat/os_support.h"

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/types.h>
#include <sys/resource.h>
#elif defined(HAVE_GETPROCESSTIMES)
#include <windows.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#elif defined(HAVE_CONIO_H)
#include <conio.h>
#endif
#undef time //needed because HAVE_AV_CONFIG_H is defined on top
#include <time.h>

#include "cmdutils.h"

#undef NDEBUG
#include <assert.h>

#undef exit

const char program_name[] = "FFmpeg";
const int program_birth_year = 2000;

/* select an input stream for an output stream */
typedef struct AVStreamMap {
    int file_index;
    int stream_index;
    int sync_file_index;
    int sync_stream_index;
} AVStreamMap;

/** select an input file for an output file */
typedef struct AVMetaDataMap {
    int out_file;
    int in_file;
} AVMetaDataMap;

static const OptionDef options[];

#define MAX_FILES 20

static AVFormatContext *input_files[MAX_FILES];
static int64_t input_files_ts_offset[MAX_FILES];
static double input_files_ts_scale[MAX_FILES][MAX_STREAMS];
static AVCodec *input_codecs[MAX_FILES*MAX_STREAMS];
static int nb_input_files = 0;
static int nb_icodecs;

static AVFormatContext *output_files[MAX_FILES];
static AVCodec *output_codecs[MAX_FILES*MAX_STREAMS];
static int nb_output_files = 0;
static int nb_ocodecs;

static AVStreamMap stream_maps[MAX_FILES*MAX_STREAMS];
static int nb_stream_maps;

static AVMetaDataMap meta_data_maps[MAX_FILES];
static int nb_meta_data_maps;

static AVInputFormat *file_iformat;
static AVOutputFormat *file_oformat;
static int frame_width  = 0;
static int frame_height = 0;
static float frame_aspect_ratio = 0;
static enum PixelFormat frame_pix_fmt = PIX_FMT_NONE;
static enum SampleFormat audio_sample_fmt = SAMPLE_FMT_NONE;
static int frame_padtop  = 0;
static int frame_padbottom = 0;
static int frame_padleft  = 0;
static int frame_padright = 0;
static int padcolor[3] = {16,128,128}; /* default to black */
static int frame_topBand  = 0;
static int frame_bottomBand = 0;
static int frame_leftBand  = 0;
static int frame_rightBand = 0;
static int max_frames[4] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
static AVRational frame_rate;
static float video_qscale = 0;
static uint16_t *intra_matrix = NULL;
static uint16_t *inter_matrix = NULL;
#if 0 //experimental, (can be removed)
static float video_rc_qsquish=1.0;
static float video_rc_qmod_amp=0;
static int video_rc_qmod_freq=0;
#endif
static const char *video_rc_override_string=NULL;
static int video_disable = 0;
static int video_discard = 0;
static char *video_codec_name = NULL;
static int video_codec_tag = 0;
static int same_quality = 0;
static int do_deinterlace = 0;
static int top_field_first = -1;
static int me_threshold = 0;
static int intra_dc_precision = 8;
static int loop_input = 0;
static int loop_output = AVFMT_NOOUTPUTLOOP;
static int qp_hist = 0;

static int intra_only = 0;
static int audio_sample_rate = 44100;
static int64_t channel_layout = 0;
#define QSCALE_NONE -99999
static float audio_qscale = QSCALE_NONE;
static int audio_disable = 0;
static int audio_channels = 1;
static char  *audio_codec_name = NULL;
static int audio_codec_tag = 0;
static char *audio_language = NULL;

static int subtitle_disable = 0;
static char *subtitle_codec_name = NULL;
static char *subtitle_language = NULL;

static float mux_preload= 0.5;
static float mux_max_delay= 0.7;

static int64_t recording_time = INT64_MAX;
static int64_t start_time = 0;
static int64_t rec_timestamp = 0;
static int64_t input_ts_offset = 0;
static int file_overwrite = 0;
static char *str_title = NULL;
static char *str_author = NULL;
static char *str_copyright = NULL;
static char *str_comment = NULL;
static char *str_genre = NULL;
static char *str_album = NULL;
static int do_benchmark = 0;
static int do_hex_dump = 0;
static int do_pkt_dump = 0;
static int do_psnr = 0;
static int do_pass = 0;
static char *pass_logfilename = NULL;
static int audio_stream_copy = 0;
static int video_stream_copy = 0;
static int subtitle_stream_copy = 0;
static int video_sync_method= -1;
static int audio_sync_method= 0;
static float audio_drift_threshold= 0.1;
static int copy_ts= 0;
static int opt_shortest = 0; //
static int video_global_header = 0;
static char *vstats_filename;
static FILE *vstats_file;
static int opt_programid = 0;

static int rate_emu = 0;

static int  video_channel = 0;
static char *video_standard;

static int audio_volume = 256;

static int exit_on_error = 0;
static int using_stdin = 0;
static int using_vhook = 0;
static int verbose = 1;
static int thread_count= 1;
static int q_pressed = 0;
static int64_t video_size = 0;
static int64_t audio_size = 0;
static int64_t extra_size = 0;
static int nb_frames_dup = 0;
static int nb_frames_drop = 0;
static int input_sync;
static uint64_t limit_filesize = 0; //
static int force_fps = 0;

static int pgmyuv_compatibility_hack=0;
static float dts_delta_threshold = 10;

static unsigned int sws_flags = SWS_BICUBIC;

static int64_t timer_start;

static AVBitStreamFilterContext *video_bitstream_filters=NULL;
static AVBitStreamFilterContext *audio_bitstream_filters=NULL;
static AVBitStreamFilterContext *subtitle_bitstream_filters=NULL;
static AVBitStreamFilterContext *bitstream_filters[MAX_FILES][MAX_STREAMS];

#define DEFAULT_PASS_LOGFILENAME "ffmpeg2pass"

struct AVInputStream;

typedef struct AVOutputStream {
    int file_index;          /* file index */
    int index;               /* stream index in the output file */
    int source_index;        /* AVInputStream index */
    AVStream *st;            /* stream in the output file */
    int encoding_needed;     /* true if encoding needed for this stream */
    int frame_number;
    /* input pts and corresponding output pts
       for A/V sync */
    //double sync_ipts;        /* dts from the AVPacket of the demuxer in second units */
    struct AVInputStream *sync_ist; /* input stream to sync against */
    int64_t sync_opts;       /* output frame counter, could be changed to some true timestamp */ //FIXME look at frame_number
    /* video only */
    int video_resample;
    AVFrame pict_tmp;      /* temporary image for resampling */
    struct SwsContext *img_resample_ctx; /* for image resampling */
    int resample_height;

    int video_crop;
    int topBand;             /* cropping area sizes */
    int leftBand;

    int video_pad;
    int padtop;              /* padding area sizes */
    int padbottom;
    int padleft;
    int padright;

    /* audio only */
    int audio_resample;
    ReSampleContext *resample; /* for audio resampling */
    int reformat_pair;
    AVAudioConvert *reformat_ctx;
    AVFifoBuffer fifo;     /* for compression: one audio fifo per codec */
    FILE *logfile;
} AVOutputStream;

typedef struct AVInputStream {
    int file_index;
    int index;
    AVStream *st;
    int discard;             /* true if stream data should be discarded */
    int decoding_needed;     /* true if the packets must be decoded in 'raw_fifo' */
    int64_t sample_index;      /* current sample */

    int64_t       start;     /* time when read started */
    int64_t       next_pts;  /* synthetic pts for cases where pkt.pts
                                is not defined */
    int64_t       pts;       /* current pts */
    int is_start;            /* is 1 at the start and after a discontinuity */
} AVInputStream;

typedef struct AVInputFile {
    int eof_reached;      /* true if eof reached */
    int ist_index;        /* index of first stream in ist_table */
    int buffer_size;      /* current total buffer size */
    int nb_streams;       /* nb streams we are aware of */
} AVInputFile;

#ifdef HAVE_TERMIOS_H

/* init terminal so that we can grab keys */
static struct termios oldtty;
#endif

static void term_exit(void)
{
#ifdef HAVE_TERMIOS_H
    tcsetattr (0, TCSANOW, &oldtty);
#endif
}

static volatile sig_atomic_t received_sigterm = 0;

static void
sigterm_handler(int sig)
{
    received_sigterm = sig;
    term_exit();
}

static void term_init(void)
{
#ifdef HAVE_TERMIOS_H
    struct termios tty;

    tcgetattr (0, &tty);
    oldtty = tty;

    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                          |INLCR|IGNCR|ICRNL|IXON);
    tty.c_oflag |= OPOST;
    tty.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN);
    tty.c_cflag &= ~(CSIZE|PARENB);
    tty.c_cflag |= CS8;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    tcsetattr (0, TCSANOW, &tty);
    signal(SIGQUIT, sigterm_handler); /* Quit (POSIX).  */
#endif

    signal(SIGINT , sigterm_handler); /* Interrupt (ANSI).  */
    signal(SIGTERM, sigterm_handler); /* Termination (ANSI).  */
    /*
    register a function to be called at normal program termination
    */
    atexit(term_exit);
#ifdef CONFIG_BEOS_NETSERVER
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
#endif
}

/* read a key without blocking */
static int read_key(void)
{
#if defined(HAVE_TERMIOS_H)
    int n = 1;
    unsigned char ch;
#ifndef CONFIG_BEOS_NETSERVER
    struct timeval tv;
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    n = select(1, &rfds, NULL, NULL, &tv);
#endif
    if (n > 0) {
        n = read(0, &ch, 1);
        if (n == 1)
            return ch;

        return n;
    }
#elif defined(HAVE_CONIO_H)
    if(kbhit())
        return(getch());
#endif
    return -1;
}

static int decode_interrupt_cb(void)
{
    return q_pressed || (q_pressed = read_key() == 'q');
}

static int av_exit(int ret)
{
    int i;

    /* close files */
    for(i=0;i<nb_output_files;i++) {
        /* maybe av_close_output_file ??? */
        AVFormatContext *s = output_files[i];
        int j;
        if (!(s->oformat->flags & AVFMT_NOFILE) && s->pb)
            url_fclose(s->pb);
        for(j=0;j<s->nb_streams;j++) {
            av_free(s->streams[j]->codec);
            av_free(s->streams[j]);
        }
        av_free(s);
    }
    for(i=0;i<nb_input_files;i++)
        av_close_input_file(input_files[i]);

    av_free(intra_matrix);
    av_free(inter_matrix);

    if (vstats_file)
        fclose(vstats_file);
    av_free(vstats_filename);

    av_free(opt_names);

    av_free(video_codec_name);
    av_free(audio_codec_name);
    av_free(subtitle_codec_name);

    av_free(video_standard);

#ifdef CONFIG_POWERPC_PERF
    void powerpc_display_perf_report(void);
    powerpc_display_perf_report();
#endif /* CONFIG_POWERPC_PERF */

    if (received_sigterm) {
        fprintf(stderr,
            "Received signal %d: terminating.\n",
            (int) received_sigterm);
        exit (255);
    }

    exit(ret); /* not all OS-es handle main() return value */
    return ret;
}

static int read_ffserver_streams(AVFormatContext *s, const char *filename)
{
    int i, err;
    AVFormatContext *ic;
    int nopts = 0;

    err = av_open_input_file(&ic, filename, NULL, FFM_PACKET_SIZE, NULL);
    if (err < 0)
        return err;
    /* copy stream format */
    s->nb_streams = ic->nb_streams;
    for(i=0;i<ic->nb_streams;i++) {
        AVStream *st;

        // FIXME: a more elegant solution is needed
        st = av_mallocz(sizeof(AVStream));
        memcpy(st, ic->streams[i], sizeof(AVStream));
        st->codec = avcodec_alloc_context();
        memcpy(st->codec, ic->streams[i]->codec, sizeof(AVCodecContext));
        s->streams[i] = st;

        if (st->codec->codec_type == CODEC_TYPE_AUDIO && audio_stream_copy)
            st->stream_copy = 1;
        else if (st->codec->codec_type == CODEC_TYPE_VIDEO && video_stream_copy)
            st->stream_copy = 1;

        if(!st->codec->thread_count)
            st->codec->thread_count = 1;
        if(st->codec->thread_count>1)
            avcodec_thread_init(st->codec, st->codec->thread_count);

        if(st->codec->flags & CODEC_FLAG_BITEXACT)
            nopts = 1;
    }

    if (!nopts)
        s->timestamp = av_gettime();

    av_close_input_file(ic);
    return 0;
}

static double
get_sync_ipts(const AVOutputStream *ost)
{
    const AVInputStream *ist = ost->sync_ist;
    return (double)(ist->pts - start_time)/AV_TIME_BASE;
}

static void write_frame(AVFormatContext *s, AVPacket *pkt, AVCodecContext *avctx, AVBitStreamFilterContext *bsfc){
    int ret;

    while(bsfc){
        AVPacket new_pkt= *pkt;
        int a= av_bitstream_filter_filter(bsfc, avctx, NULL,
                                          &new_pkt.data, &new_pkt.size,
                                          pkt->data, pkt->size,
                                          pkt->flags & PKT_FLAG_KEY);
        if(a>0){
            av_free_packet(pkt);
            new_pkt.destruct= av_destruct_packet;
        } else if(a<0){
            fprintf(stderr, "%s failed for stream %d, codec %s",
                    bsfc->filter->name, pkt->stream_index,
                    avctx->codec ? avctx->codec->name : "copy");
            print_error("", a);
            if (exit_on_error)
                av_exit(1);
        }
        *pkt= new_pkt;

        bsfc= bsfc->next;
    }

    ret= av_interleaved_write_frame(s, pkt);
    if(ret < 0){
        print_error("av_interleaved_write_frame()", ret);
        av_exit(1);
    }
}

#define MAX_AUDIO_PACKET_SIZE (128 * 1024)

static void do_audio_out(AVFormatContext *s,
                         AVOutputStream *ost,
                         AVInputStream *ist,
                         unsigned char *buf, int size)
{
    uint8_t *buftmp;
    static uint8_t *audio_buf = NULL;
    static uint8_t *audio_out = NULL;
    static uint8_t *audio_out2 = NULL;
    const int audio_out_size= 4*MAX_AUDIO_PACKET_SIZE;

    int size_out, frame_bytes, ret;
    AVCodecContext *enc= ost->st->codec;
    AVCodecContext *dec= ist->st->codec;
    int osize= av_get_bits_per_sample_format(enc->sample_fmt)/8;
    int isize= av_get_bits_per_sample_format(dec->sample_fmt)/8;

    /* SC: dynamic allocation of buffers */
    if (!audio_buf)
        audio_buf = av_malloc(2*MAX_AUDIO_PACKET_SIZE);
    if (!audio_out)
        audio_out = av_malloc(audio_out_size);
    if (!audio_buf || !audio_out)
        return;               /* Should signal an error ! */

    if (enc->channels != dec->channels)
        ost->audio_resample = 1;

    if (ost->audio_resample && !ost->resample) {
        if (dec->sample_fmt != SAMPLE_FMT_S16) {
            fprintf(stderr, "Audio resampler only works with 16 bits per sample, patch welcome.\n");
            av_exit(1);
        }
        ost->resample = audio_resample_init(enc->channels,    dec->channels,
                                            enc->sample_rate, dec->sample_rate);
        if (!ost->resample) {
            fprintf(stderr, "Can not resample %d channels @ %d Hz to %d channels @ %d Hz\n",
                    dec->channels, dec->sample_rate,
                    enc->channels, enc->sample_rate);
            av_exit(1);
        }
    }

#define MAKE_SFMT_PAIR(a,b) ((a)+SAMPLE_FMT_NB*(b))
    if (dec->sample_fmt!=enc->sample_fmt &&
        MAKE_SFMT_PAIR(enc->sample_fmt,dec->sample_fmt)!=ost->reformat_pair) {
        if (!audio_out2)
            audio_out2 = av_malloc(audio_out_size);
        if (!audio_out2)
            av_exit(1);
        if (ost->reformat_ctx)
            av_audio_convert_free(ost->reformat_ctx);
        ost->reformat_ctx = av_audio_convert_alloc(enc->sample_fmt, 1,
                                                   dec->sample_fmt, 1, NULL, 0);
        if (!ost->reformat_ctx) {
            fprintf(stderr, "Cannot convert %s sample format to %s sample format\n",
                avcodec_get_sample_fmt_name(dec->sample_fmt),
                avcodec_get_sample_fmt_name(enc->sample_fmt));
            av_exit(1);
        }
        ost->reformat_pair=MAKE_SFMT_PAIR(enc->sample_fmt,dec->sample_fmt);
    }

    if(audio_sync_method){
        double delta = get_sync_ipts(ost) * enc->sample_rate - ost->sync_opts
                - av_fifo_size(&ost->fifo)/(ost->st->codec->channels * 2);
        double idelta= delta*ist->st->codec->sample_rate / enc->sample_rate;
        int byte_delta= ((int)idelta)*2*ist->st->codec->channels;

        //FIXME resample delay
        if(fabs(delta) > 50){
            if(ist->is_start || fabs(delta) > audio_drift_threshold*enc->sample_rate){
                if(byte_delta < 0){
                    byte_delta= FFMAX(byte_delta, -size);
                    size += byte_delta;
                    buf  -= byte_delta;
                    if(verbose > 2)
                        fprintf(stderr, "discarding %d audio samples\n", (int)-delta);
                    if(!size)
                        return;
                    ist->is_start=0;
                }else{
                    static uint8_t *input_tmp= NULL;
                    input_tmp= av_realloc(input_tmp, byte_delta + size);

                    if(byte_delta + size <= MAX_AUDIO_PACKET_SIZE)
                        ist->is_start=0;
                    else
                        byte_delta= MAX_AUDIO_PACKET_SIZE - size;

                    memset(input_tmp, 0, byte_delta);
                    memcpy(input_tmp + byte_delta, buf, size);
                    buf= input_tmp;
                    size += byte_delta;
                    if(verbose > 2)
                        fprintf(stderr, "adding %d audio samples of silence\n", (int)delta);
                }
            }else if(audio_sync_method>1){
                int comp= av_clip(delta, -audio_sync_method, audio_sync_method);
                assert(ost->audio_resample);
                if(verbose > 2)
                    fprintf(stderr, "compensating audio timestamp drift:%f compensation:%d in:%d\n", delta, comp, enc->sample_rate);
//                fprintf(stderr, "drift:%f len:%d opts:%"PRId64" ipts:%"PRId64" fifo:%d\n", delta, -1, ost->sync_opts, (int64_t)(get_sync_ipts(ost) * enc->sample_rate), av_fifo_size(&ost->fifo)/(ost->st->codec->channels * 2));
                av_resample_compensate(*(struct AVResampleContext**)ost->resample, comp, enc->sample_rate);
            }
        }
    }else
        ost->sync_opts= lrintf(get_sync_ipts(ost) * enc->sample_rate)
                        - av_fifo_size(&ost->fifo)/(ost->st->codec->channels * 2); //FIXME wrong

    if (ost->audio_resample) {
        buftmp = audio_buf;
        size_out = audio_resample(ost->resample,
                                  (short *)buftmp, (short *)buf,
                                  size / (ist->st->codec->channels * isize));
        size_out = size_out * enc->channels * osize;
    } else {
        buftmp = buf;
        size_out = size;
    }

    if (dec->sample_fmt!=enc->sample_fmt) {
        const void *ibuf[6]= {buftmp};
        void *obuf[6]= {audio_out2};
        int istride[6]= {isize};
        int ostride[6]= {osize};
        int len= size_out/istride[0];
        if (av_audio_convert(ost->reformat_ctx, obuf, ostride, ibuf, istride, len)<0) {
            printf("av_audio_convert() failed\n");
            if (exit_on_error)
                av_exit(1);
            return;
        }
        buftmp = audio_out2;
        size_out = len*osize;
    }

    /* now encode as many frames as possible */
    if (enc->frame_size > 1) {
        /* output resampled raw samples */
        if (av_fifo_realloc2(&ost->fifo, av_fifo_size(&ost->fifo) + size_out) < 0) {
            fprintf(stderr, "av_fifo_realloc2() failed\n");
            av_exit(1);
        }
        av_fifo_generic_write(&ost->fifo, buftmp, size_out, NULL);

        frame_bytes = enc->frame_size * osize * enc->channels;

        while (av_fifo_size(&ost->fifo) >= frame_bytes) {
            AVPacket pkt;
            av_init_packet(&pkt);

            av_fifo_read(&ost->fifo, audio_buf, frame_bytes);

            //FIXME pass ost->sync_opts as AVFrame.pts in avcodec_encode_audio()

            ret = avcodec_encode_audio(enc, audio_out, audio_out_size,
                                       (short *)audio_buf);
            audio_size += ret;
            pkt.stream_index= ost->index;
            pkt.data= audio_out;
            pkt.size= ret;
            if(enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
                pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ost->st->time_base);
            pkt.flags |= PKT_FLAG_KEY;
            write_frame(s, &pkt, ost->st->codec, bitstream_filters[ost->file_index][pkt.stream_index]);

            ost->sync_opts += enc->frame_size;
        }
    } else {
        AVPacket pkt;
        int coded_bps = av_get_bits_per_sample(enc->codec->id)/8;
        av_init_packet(&pkt);

        ost->sync_opts += size_out / (osize * enc->channels);

        /* output a pcm frame */
        /* determine the size of the coded buffer */
        size_out /= osize;
        if (coded_bps)
            size_out *= coded_bps;

        //FIXME pass ost->sync_opts as AVFrame.pts in avcodec_encode_audio()
        ret = avcodec_encode_audio(enc, audio_out, size_out,
                                   (short *)buftmp);
        audio_size += ret;
        pkt.stream_index= ost->index;
        pkt.data= audio_out;
        pkt.size= ret;
        if(enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
            pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ost->st->time_base);
        pkt.flags |= PKT_FLAG_KEY;
        write_frame(s, &pkt, ost->st->codec, bitstream_filters[ost->file_index][pkt.stream_index]);
    }
}

static void pre_process_video_frame(AVInputStream *ist, AVPicture *picture, void **bufp)
{
    AVCodecContext *dec;
    AVPicture *picture2;
    AVPicture picture_tmp;
    uint8_t *buf = 0;

    dec = ist->st->codec;

    /* deinterlace : must be done before any resize */
    if (do_deinterlace || using_vhook) {
        int size;

        /* create temporary picture */
        size = avpicture_get_size(dec->pix_fmt, dec->width, dec->height);
        buf = av_malloc(size);
        if (!buf)
            return;

        picture2 = &picture_tmp;
        avpicture_fill(picture2, buf, dec->pix_fmt, dec->width, dec->height);

        if (do_deinterlace){
            if(avpicture_deinterlace(picture2, picture,
                                     dec->pix_fmt, dec->width, dec->height) < 0) {
                /* if error, do not deinterlace */
                fprintf(stderr, "Deinterlacing failed\n");
                av_free(buf);
                buf = NULL;
                picture2 = picture;
            }
        } else {
            av_picture_copy(picture2, picture, dec->pix_fmt, dec->width, dec->height);
        }
    } else {
        picture2 = picture;
    }

    if (ENABLE_VHOOK)
        frame_hook_process(picture2, dec->pix_fmt, dec->width, dec->height,
                           1000000 * ist->pts / AV_TIME_BASE);

    if (picture != picture2)
        *picture = *picture2;
    *bufp = buf;
}

/* we begin to correct av delay at this threshold */
#define AV_DELAY_MAX 0.100

static void do_subtitle_out(AVFormatContext *s,
                            AVOutputStream *ost,
                            AVInputStream *ist,
                            AVSubtitle *sub,
                            int64_t pts)
{
    static uint8_t *subtitle_out = NULL;
    int subtitle_out_max_size = 65536;
    int subtitle_out_size, nb, i;
    AVCodecContext *enc;
    AVPacket pkt;

    if (pts == AV_NOPTS_VALUE) {
        fprintf(stderr, "Subtitle packets must have a pts\n");
        if (exit_on_error)
            av_exit(1);
        return;
    }

    enc = ost->st->codec;

    if (!subtitle_out) {
        subtitle_out = av_malloc(subtitle_out_max_size);
    }

    /* Note: DVB subtitle need one packet to draw them and one other
       packet to clear them */
    /* XXX: signal it in the codec context ? */
    if (enc->codec_id == CODEC_ID_DVB_SUBTITLE)
        nb = 2;
    else
        nb = 1;

    for(i = 0; i < nb; i++) {
        subtitle_out_size = avcodec_encode_subtitle(enc, subtitle_out,
                                                    subtitle_out_max_size, sub);

        av_init_packet(&pkt);
        pkt.stream_index = ost->index;
        pkt.data = subtitle_out;
        pkt.size = subtitle_out_size;
        pkt.pts = av_rescale_q(pts, ist->st->time_base, ost->st->time_base);
        if (enc->codec_id == CODEC_ID_DVB_SUBTITLE) {
            /* XXX: the pts correction is handled here. Maybe handling
               it in the codec would be better */
            if (i == 0)
                pkt.pts += 90 * sub->start_display_time;
            else
                pkt.pts += 90 * sub->end_display_time;
        }
        write_frame(s, &pkt, ost->st->codec, bitstream_filters[ost->file_index][pkt.stream_index]);
    }
}

static int bit_buffer_size= 1024*256;
static uint8_t *bit_buffer= NULL;

static void do_video_out(AVFormatContext *s,
                         AVOutputStream *ost,
                         AVInputStream *ist,
                         AVFrame *in_picture,
                         int *frame_size)
{
    int nb_frames, i, ret;
    AVFrame *final_picture, *formatted_picture, *resampling_dst, *padding_src;
    AVFrame picture_crop_temp, picture_pad_temp;
    AVCodecContext *enc, *dec;

    avcodec_get_frame_defaults(&picture_crop_temp);
    avcodec_get_frame_defaults(&picture_pad_temp);

    enc = ost->st->codec;
    dec = ist->st->codec;

    /* by default, we output a single frame */
    nb_frames = 1;

    *frame_size = 0;

    if(video_sync_method>0 || (video_sync_method && av_q2d(enc->time_base) > 0.001)){
        double vdelta;
        vdelta = get_sync_ipts(ost) / av_q2d(enc->time_base) - ost->sync_opts;
        //FIXME set to 0.5 after we fix some dts/pts bugs like in avidec.c
        if (vdelta < -1.1)
            nb_frames = 0;
        else if (video_sync_method == 2)
            ost->sync_opts= lrintf(get_sync_ipts(ost) / av_q2d(enc->time_base));
        else if (vdelta > 1.1)
            nb_frames = lrintf(vdelta);
//fprintf(stderr, "vdelta:%f, ost->sync_opts:%"PRId64", ost->sync_ipts:%f nb_frames:%d\n", vdelta, ost->sync_opts, ost->sync_ipts, nb_frames);
        if (nb_frames == 0){
            ++nb_frames_drop;
            if (verbose>2)
                fprintf(stderr, "*** drop!\n");
        }else if (nb_frames > 1) {
            nb_frames_dup += nb_frames;
            if (verbose>2)
                fprintf(stderr, "*** %d dup!\n", nb_frames-1);
        }
    }else
        ost->sync_opts= lrintf(get_sync_ipts(ost) / av_q2d(enc->time_base));

    nb_frames= FFMIN(nb_frames, max_frames[CODEC_TYPE_VIDEO] - ost->frame_number);
    if (nb_frames <= 0)
        return;

    if (ost->video_crop) {
        if (av_picture_crop((AVPicture *)&picture_crop_temp, (AVPicture *)in_picture, dec->pix_fmt, ost->topBand, ost->leftBand) < 0) {
            av_log(NULL, AV_LOG_ERROR, "error cropping picture\n");
            if (exit_on_error)
                av_exit(1);
            return;
        }
        formatted_picture = &picture_crop_temp;
    } else {
        formatted_picture = in_picture;
    }

    final_picture = formatted_picture;
    padding_src = formatted_picture;
    resampling_dst = &ost->pict_tmp;
    if (ost->video_pad) {
        final_picture = &ost->pict_tmp;
        if (ost->video_resample) {
            if (av_picture_crop((AVPicture *)&picture_pad_temp, (AVPicture *)final_picture, enc->pix_fmt, ost->padtop, ost->padleft) < 0) {
                av_log(NULL, AV_LOG_ERROR, "error padding picture\n");
                if (exit_on_error)
                    av_exit(1);
                return;
            }
            resampling_dst = &picture_pad_temp;
        }
    }

    if (ost->video_resample) {
        padding_src = NULL;
        final_picture = &ost->pict_tmp;
        sws_scale(ost->img_resample_ctx, formatted_picture->data, formatted_picture->linesize,
              0, ost->resample_height, resampling_dst->data, resampling_dst->linesize);
    }

    if (ost->video_pad) {
        av_picture_pad((AVPicture*)final_picture, (AVPicture *)padding_src,
                enc->height, enc->width, enc->pix_fmt,
                ost->padtop, ost->padbottom, ost->padleft, ost->padright, padcolor);
    }

    /* duplicates frame if needed */
    for(i=0;i<nb_frames;i++) {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.stream_index= ost->index;

        if (s->oformat->flags & AVFMT_RAWPICTURE) {
            /* raw pictures are written as AVPicture structure to
               avoid any copies. We support temorarily the older
               method. */
            AVFrame* old_frame = enc->coded_frame;
            enc->coded_frame = dec->coded_frame; //FIXME/XXX remove this hack
            pkt.data= (uint8_t *)final_picture;
            pkt.size=  sizeof(AVPicture);
            pkt.pts= av_rescale_q(ost->sync_opts, enc->time_base, ost->st->time_base);
            pkt.flags |= PKT_FLAG_KEY;

            write_frame(s, &pkt, ost->st->codec, bitstream_filters[ost->file_index][pkt.stream_index]);
            enc->coded_frame = old_frame;
        } else {
            AVFrame big_picture;

            big_picture= *final_picture;
            /* better than nothing: use input picture interlaced
               settings */
            big_picture.interlaced_frame = in_picture->interlaced_frame;
            if(avctx_opts[CODEC_TYPE_VIDEO]->flags & (CODEC_FLAG_INTERLACED_DCT|CODEC_FLAG_INTERLACED_ME)){
                if(top_field_first == -1)
                    big_picture.top_field_first = in_picture->top_field_first;
                else
                    big_picture.top_field_first = top_field_first;
            }

            /* handles sameq here. This is not correct because it may
               not be a global option */
            if (same_quality) {
                big_picture.quality = ist->st->quality;
            }else
                big_picture.quality = ost->st->quality;
            if(!me_threshold)
                big_picture.pict_type = 0;
//            big_picture.pts = AV_NOPTS_VALUE;
            big_picture.pts= ost->sync_opts;
//            big_picture.pts= av_rescale(ost->sync_opts, AV_TIME_BASE*(int64_t)enc->time_base.num, enc->time_base.den);
//av_log(NULL, AV_LOG_DEBUG, "%"PRId64" -> encoder\n", ost->sync_opts);
            ret = avcodec_encode_video(enc,
                                       bit_buffer, bit_buffer_size,
                                       &big_picture);
            if (ret == -1) {
                fprintf(stderr, "Video encoding failed\n");
                av_exit(1);
            }
            //enc->frame_number = enc->real_pict_num;
            if(ret>0){
                pkt.data= bit_buffer;
                pkt.size= ret;
                if(enc->coded_frame->pts != AV_NOPTS_VALUE)
                    pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ost->st->time_base);
/*av_log(NULL, AV_LOG_DEBUG, "encoder -> %"PRId64"/%"PRId64"\n",
   pkt.pts != AV_NOPTS_VALUE ? av_rescale(pkt.pts, enc->time_base.den, AV_TIME_BASE*(int64_t)enc->time_base.num) : -1,
   pkt.dts != AV_NOPTS_VALUE ? av_rescale(pkt.dts, enc->time_base.den, AV_TIME_BASE*(int64_t)enc->time_base.num) : -1);*/

                if(enc->coded_frame->key_frame)
                    pkt.flags |= PKT_FLAG_KEY;
                write_frame(s, &pkt, ost->st->codec, bitstream_filters[ost->file_index][pkt.stream_index]);
                *frame_size = ret;
                video_size += ret;
                //fprintf(stderr,"\nFrame: %3d %3d size: %5d type: %d",
                //        enc->frame_number-1, enc->real_pict_num, ret,
                //        enc->pict_type);
                /* if two pass, output log */
                if (ost->logfile && enc->stats_out) {
                    fprintf(ost->logfile, "%s", enc->stats_out);
                }
            }
        }
        ost->sync_opts++;
        ost->frame_number++;
    }
}

static double psnr(double d){
    return -10.0*log(d)/log(10.0);
}

static void do_video_stats(AVFormatContext *os, AVOutputStream *ost,
                           int frame_size)
{
    AVCodecContext *enc;
    int frame_number;
    double ti1, bitrate, avg_bitrate;

    /* this is executed just the first time do_video_stats is called */
    if (!vstats_file) {
        vstats_file = fopen(vstats_filename, "w");
        if (!vstats_file) {
            perror("fopen");
            av_exit(1);
        }
    }

    enc = ost->st->codec;
    if (enc->codec_type == CODEC_TYPE_VIDEO) {
        frame_number = ost->frame_number;
        fprintf(vstats_file, "frame= %5d q= %2.1f ", frame_number, enc->coded_frame->quality/(float)FF_QP2LAMBDA);
        if (enc->flags&CODEC_FLAG_PSNR)
            fprintf(vstats_file, "PSNR= %6.2f ", psnr(enc->coded_frame->error[0]/(enc->width*enc->height*255.0*255.0)));

        fprintf(vstats_file,"f_size= %6d ", frame_size);
        /* compute pts value */
        ti1 = ost->sync_opts * av_q2d(enc->time_base);
        if (ti1 < 0.01)
            ti1 = 0.01;

        bitrate = (frame_size * 8) / av_q2d(enc->time_base) / 1000.0;
        avg_bitrate = (double)(video_size * 8) / ti1 / 1000.0;
        fprintf(vstats_file, "s_size= %8.0fkB time= %0.3f br= %7.1fkbits/s avg_br= %7.1fkbits/s ",
            (double)video_size / 1024, ti1, bitrate, avg_bitrate);
        fprintf(vstats_file,"type= %c\n", av_get_pict_type_char(enc->coded_frame->pict_type));
    }
}

static void print_report(AVFormatContext **output_files,
                         AVOutputStream **ost_table, int nb_ostreams,
                         int is_last_report)
{
    char buf[1024];
    AVOutputStream *ost;
    AVFormatContext *oc, *os;
    int64_t total_size;
    AVCodecContext *enc;
    int frame_number, vid, i;
    double bitrate, ti1, pts;
    static int64_t last_time = -1;
    static int qp_histogram[52];

    if (!is_last_report) {
        int64_t cur_time;
        /* display the report every 0.5 seconds */
        cur_time = av_gettime();
        if (last_time == -1) {
            last_time = cur_time;
            return;
        }
        if ((cur_time - last_time) < 500000)
            return;
        last_time = cur_time;
    }


    oc = output_files[0];

    total_size = url_fsize(oc->pb);
    if(total_size<0) // FIXME improve url_fsize() so it works with non seekable output too
        total_size= url_ftell(oc->pb);

    buf[0] = '\0';
    ti1 = 1e10;
    vid = 0;
    for(i=0;i<nb_ostreams;i++) {
        ost = ost_table[i];
        os = output_files[ost->file_index];
        enc = ost->st->codec;
        if (vid && enc->codec_type == CODEC_TYPE_VIDEO) {
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "q=%2.1f ",
                     !ost->st->stream_copy ?
                     enc->coded_frame->quality/(float)FF_QP2LAMBDA : -1);
        }
        if (!vid && enc->codec_type == CODEC_TYPE_VIDEO) {
            float t = (av_gettime()-timer_start) / 1000000.0;

            frame_number = ost->frame_number;
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "frame=%5d fps=%3d q=%3.1f ",
                     frame_number, (t>1)?(int)(frame_number/t+0.5) : 0,
                     !ost->st->stream_copy ?
                     enc->coded_frame->quality/(float)FF_QP2LAMBDA : -1);
            if(is_last_report)
                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "L");
            if(qp_hist){
                int j;
                int qp= lrintf(enc->coded_frame->quality/(float)FF_QP2LAMBDA);
                if(qp>=0 && qp<FF_ARRAY_ELEMS(qp_histogram))
                    qp_histogram[qp]++;
                for(j=0; j<32; j++)
                    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%X", (int)lrintf(log(qp_histogram[j]+1)/log(2)));
            }
            if (enc->flags&CODEC_FLAG_PSNR){
                int j;
                double error, error_sum=0;
                double scale, scale_sum=0;
                char type[3]= {'Y','U','V'};
                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "PSNR=");
                for(j=0; j<3; j++){
                    if(is_last_report){
                        error= enc->error[j];
                        scale= enc->width*enc->height*255.0*255.0*frame_number;
                    }else{
                        error= enc->coded_frame->error[j];
                        scale= enc->width*enc->height*255.0*255.0;
                    }
                    if(j) scale/=4;
                    error_sum += error;
                    scale_sum += scale;
                    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c:%2.2f ", type[j], psnr(error/scale));
                }
                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "*:%2.2f ", psnr(error_sum/scale_sum));
            }
            vid = 1;
        }
        /* compute min output value */
        pts = (double)ost->st->pts.val * av_q2d(ost->st->time_base);
        if ((pts < ti1) && (pts > 0))
            ti1 = pts;
    }
    if (ti1 < 0.01)
        ti1 = 0.01;

    if (verbose || is_last_report) {
        bitrate = (double)(total_size * 8) / ti1 / 1000.0;

        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
            "size=%8.0fkB time=%0.2f bitrate=%6.1fkbits/s",
            (double)total_size / 1024, ti1, bitrate);

        if (verbose > 1)
          snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " dup=%d drop=%d",
                  nb_frames_dup, nb_frames_drop);

        if (verbose >= 0)
            fprintf(stderr, "%s    \r", buf);

        fflush(stderr);
    }

    if (is_last_report && verbose >= 0){
        int64_t raw= audio_size + video_size + extra_size;
        fprintf(stderr, "\n");
        fprintf(stderr, "video:%1.0fkB audio:%1.0fkB global headers:%1.0fkB muxing overhead %f%%\n",
                video_size/1024.0,
                audio_size/1024.0,
                extra_size/1024.0,
                100.0*(total_size - raw)/raw
        );
    }
}

/* pkt = NULL means EOF (needed to flush decoder buffers) */
static int output_packet(AVInputStream *ist, int ist_index,
                         AVOutputStream **ost_table, int nb_ostreams,
                         const AVPacket *pkt)
{
    AVFormatContext *os;
    AVOutputStream *ost;
    uint8_t *ptr;
    int len, ret, i;
    uint8_t *data_buf;
    int data_size, got_picture;
    AVFrame picture;
    void *buffer_to_free;
    static unsigned int samples_size= 0;
    static short *samples= NULL;
    AVSubtitle subtitle, *subtitle_to_free;
    int got_subtitle;

    if(ist->next_pts == AV_NOPTS_VALUE)
        ist->next_pts= ist->pts;

    if (pkt == NULL) {
        /* EOF handling */
        ptr = NULL;
        len = 0;
        goto handle_eof;
    }

    if(pkt->dts != AV_NOPTS_VALUE)
        ist->next_pts = ist->pts = av_rescale_q(pkt->dts, ist->st->time_base, AV_TIME_BASE_Q);

    len = pkt->size;
    ptr = pkt->data;

    //while we have more to decode or while the decoder did output something on EOF
    while (len > 0 || (!pkt && ist->next_pts != ist->pts)) {
    handle_eof:
        ist->pts= ist->next_pts;

        if(len && len != pkt->size && verbose>0)
            fprintf(stderr, "Multiple frames in a packet from stream %d\n", pkt->stream_index);

        /* decode the packet if needed */
        data_buf = NULL; /* fail safe */
        data_size = 0;
        subtitle_to_free = NULL;
        if (ist->decoding_needed) {
            switch(ist->st->codec->codec_type) {
            case CODEC_TYPE_AUDIO:{
                if(pkt && samples_size < FFMAX(pkt->size*sizeof(*samples), AVCODEC_MAX_AUDIO_FRAME_SIZE)) {
                    samples_size = FFMAX(pkt->size*sizeof(*samples), AVCODEC_MAX_AUDIO_FRAME_SIZE);
                    av_free(samples);
                    samples= av_malloc(samples_size);
                }
                data_size= samples_size;
                    /* XXX: could avoid copy if PCM 16 bits with same
                       endianness as CPU */
                ret = avcodec_decode_audio2(ist->st->codec, samples, &data_size,
                                           ptr, len);
                if (ret < 0)
                    goto fail_decode;
                ptr += ret;
                len -= ret;
                /* Some bug in mpeg audio decoder gives */
                /* data_size < 0, it seems they are overflows */
                if (data_size <= 0) {
                    /* no audio frame */
                    continue;
                }
                data_buf = (uint8_t *)samples;
                ist->next_pts += ((int64_t)AV_TIME_BASE/2 * data_size) /
                    (ist->st->codec->sample_rate * ist->st->codec->channels);
                break;}
            case CODEC_TYPE_VIDEO:
                    data_size = (ist->st->codec->width * ist->st->codec->height * 3) / 2;
                    /* XXX: allocate picture correctly */
                    avcodec_get_frame_defaults(&picture);

                    ret = avcodec_decode_video(ist->st->codec,
                                               &picture, &got_picture, ptr, len);
                    ist->st->quality= picture.quality;
                    if (ret < 0)
                        goto fail_decode;
                    if (!got_picture) {
                        /* no picture yet */
                        goto discard_packet;
                    }
                    if (ist->st->codec->time_base.num != 0) {
                        ist->next_pts += ((int64_t)AV_TIME_BASE *
                                          ist->st->codec->time_base.num) /
                            ist->st->codec->time_base.den;
                    }
                    len = 0;
                    break;
            case CODEC_TYPE_SUBTITLE:
                ret = avcodec_decode_subtitle(ist->st->codec,
                                              &subtitle, &got_subtitle, ptr, len);
                if (ret < 0)
                    goto fail_decode;
                if (!got_subtitle) {
                    goto discard_packet;
                }
                subtitle_to_free = &subtitle;
                len = 0;
                break;
            default:
                goto fail_decode;
            }
        } else {
            switch(ist->st->codec->codec_type) {
            case CODEC_TYPE_AUDIO:
                ist->next_pts += ((int64_t)AV_TIME_BASE * ist->st->codec->frame_size) /
                    ist->st->codec->sample_rate;
                break;
            case CODEC_TYPE_VIDEO:
                if (ist->st->codec->time_base.num != 0) {
                    ist->next_pts += ((int64_t)AV_TIME_BASE *
                                      ist->st->codec->time_base.num) /
                        ist->st->codec->time_base.den;
                }
                break;
            }
            data_buf = ptr;
            data_size = len;
            ret = len;
            len = 0;
        }

        buffer_to_free = NULL;
        if (ist->st->codec->codec_type == CODEC_TYPE_VIDEO) {
            pre_process_video_frame(ist, (AVPicture *)&picture,
                                    &buffer_to_free);
        }

        // preprocess audio (volume)
        if (ist->st->codec->codec_type == CODEC_TYPE_AUDIO) {
            if (audio_volume != 256) {
                short *volp;
                volp = samples;
                for(i=0;i<(data_size / sizeof(short));i++) {
                    int v = ((*volp) * audio_volume + 128) >> 8;
                    if (v < -32768) v = -32768;
                    if (v >  32767) v = 32767;
                    *volp++ = v;
                }
            }
        }

        /* frame rate emulation */
        if (rate_emu) {
            int64_t pts = av_rescale(ist->pts, 1000000, AV_TIME_BASE);
            int64_t now = av_gettime() - ist->start;
            if (pts > now)
                usleep(pts - now);
        }

        /* if output time reached then transcode raw format,
           encode packets and output them */
        if (start_time == 0 || ist->pts >= start_time)
            for(i=0;i<nb_ostreams;i++) {
                int frame_size;

                ost = ost_table[i];
                if (ost->source_index == ist_index) {
                    os = output_files[ost->file_index];

#if 0
                    printf("%d: got pts=%0.3f %0.3f\n", i,
                           (double)pkt->pts / AV_TIME_BASE,
                           ((double)ist->pts / AV_TIME_BASE) -
                           ((double)ost->st->pts.val * ost->st->time_base.num / ost->st->time_base.den));
#endif
                    /* set the input output pts pairs */
                    //ost->sync_ipts = (double)(ist->pts + input_files_ts_offset[ist->file_index] - start_time)/ AV_TIME_BASE;

                    if (ost->encoding_needed) {
                        switch(ost->st->codec->codec_type) {
                        case CODEC_TYPE_AUDIO:
                            do_audio_out(os, ost, ist, data_buf, data_size);
                            break;
                        case CODEC_TYPE_VIDEO:
                            do_video_out(os, ost, ist, &picture, &frame_size);
                            if (vstats_filename && frame_size)
                                do_video_stats(os, ost, frame_size);
                            break;
                        case CODEC_TYPE_SUBTITLE:
                            do_subtitle_out(os, ost, ist, &subtitle,
                                            pkt->pts);
                            break;
                        default:
                            abort();
                        }
                    } else {
                        AVFrame avframe; //FIXME/XXX remove this
                        AVPacket opkt;
                        av_init_packet(&opkt);

                        if (!ost->frame_number && !(pkt->flags & PKT_FLAG_KEY))
                            continue;

                        /* no reencoding needed : output the packet directly */
                        /* force the input stream PTS */

                        avcodec_get_frame_defaults(&avframe);
                        ost->st->codec->coded_frame= &avframe;
                        avframe.key_frame = pkt->flags & PKT_FLAG_KEY;

                        if(ost->st->codec->codec_type == CODEC_TYPE_AUDIO)
                            audio_size += data_size;
                        else if (ost->st->codec->codec_type == CODEC_TYPE_VIDEO) {
                            video_size += data_size;
                            ost->sync_opts++;
                        }

                        opkt.stream_index= ost->index;
                        if(pkt->pts != AV_NOPTS_VALUE)
                            opkt.pts= av_rescale_q(pkt->pts, ist->st->time_base, ost->st->time_base);
                        else
                            opkt.pts= AV_NOPTS_VALUE;

                        if (pkt->dts == AV_NOPTS_VALUE)
                            opkt.dts = av_rescale_q(ist->pts, AV_TIME_BASE_Q, ost->st->time_base);
                        else
                            opkt.dts = av_rescale_q(pkt->dts, ist->st->time_base, ost->st->time_base);

                        opkt.duration = av_rescale_q(pkt->duration, ist->st->time_base, ost->st->time_base);
                        opkt.flags= pkt->flags;

                        //FIXME remove the following 2 lines they shall be replaced by the bitstream filters
                        if(av_parser_change(ist->st->parser, ost->st->codec, &opkt.data, &opkt.size, data_buf, data_size, pkt->flags & PKT_FLAG_KEY))
                            opkt.destruct= av_destruct_packet;

                        write_frame(os, &opkt, ost->st->codec, bitstream_filters[ost->file_index][opkt.stream_index]);
                        ost->st->codec->frame_number++;
                        ost->frame_number++;
                        av_free_packet(&opkt);
                    }
                }
            }
        av_free(buffer_to_free);
        /* XXX: allocate the subtitles in the codec ? */
        if (subtitle_to_free) {
            if (subtitle_to_free->rects != NULL) {
                for (i = 0; i < subtitle_to_free->num_rects; i++) {
                    av_free(subtitle_to_free->rects[i].bitmap);
                    av_free(subtitle_to_free->rects[i].rgba_palette);
                }
                av_freep(&subtitle_to_free->rects);
            }
            subtitle_to_free->num_rects = 0;
            subtitle_to_free = NULL;
        }
    }
 discard_packet:
    if (pkt == NULL) {
        /* EOF handling */

        for(i=0;i<nb_ostreams;i++) {
            ost = ost_table[i];
            if (ost->source_index == ist_index) {
                AVCodecContext *enc= ost->st->codec;
                os = output_files[ost->file_index];

                if(ost->st->codec->codec_type == CODEC_TYPE_AUDIO && enc->frame_size <=1)
                    continue;
                if(ost->st->codec->codec_type == CODEC_TYPE_VIDEO && (os->oformat->flags & AVFMT_RAWPICTURE))
                    continue;

                if (ost->encoding_needed) {
                    for(;;) {
                        AVPacket pkt;
                        int fifo_bytes;
                        av_init_packet(&pkt);
                        pkt.stream_index= ost->index;

                        switch(ost->st->codec->codec_type) {
                        case CODEC_TYPE_AUDIO:
                            fifo_bytes = av_fifo_size(&ost->fifo);
                            ret = 0;
                            /* encode any samples remaining in fifo */
                            if(fifo_bytes > 0 && enc->codec->capabilities & CODEC_CAP_SMALL_LAST_FRAME) {
                                int fs_tmp = enc->frame_size;
                                enc->frame_size = fifo_bytes / (2 * enc->channels);
                                av_fifo_read(&ost->fifo, (uint8_t *)samples, fifo_bytes);
                                    ret = avcodec_encode_audio(enc, bit_buffer, bit_buffer_size, samples);
                                enc->frame_size = fs_tmp;
                            }
                            if(ret <= 0) {
                                ret = avcodec_encode_audio(enc, bit_buffer, bit_buffer_size, NULL);
                            }
                            audio_size += ret;
                            pkt.flags |= PKT_FLAG_KEY;
                            break;
                        case CODEC_TYPE_VIDEO:
                            ret = avcodec_encode_video(enc, bit_buffer, bit_buffer_size, NULL);
                            video_size += ret;
                            if(enc->coded_frame && enc->coded_frame->key_frame)
                                pkt.flags |= PKT_FLAG_KEY;
                            if (ost->logfile && enc->stats_out) {
                                fprintf(ost->logfile, "%s", enc->stats_out);
                            }
                            break;
                        default:
                            ret=-1;
                        }

                        if(ret<=0)
                            break;
                        pkt.data= bit_buffer;
                        pkt.size= ret;
                        if(enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
                            pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ost->st->time_base);
                        write_frame(os, &pkt, ost->st->codec, bitstream_filters[ost->file_index][pkt.stream_index]);
                    }
                }
            }
        }
    }

    return 0;
 fail_decode:
    return -1;
}

static void print_sdp(AVFormatContext **avc, int n)
{
    char sdp[2048];

    avf_sdp_create(avc, n, sdp, sizeof(sdp));
    printf("SDP:\n%s\n", sdp);
    fflush(stdout);
}

static int stream_index_from_inputs(AVFormatContext **input_files,
                                    int nb_input_files,
                                    AVInputFile *file_table,
                                    AVInputStream **ist_table,
                                    enum CodecType type,
                                    int programid)
{
    int p, q, z;
    for(z=0; z<nb_input_files; z++) {
        AVFormatContext *ic = input_files[z];
        for(p=0; p<ic->nb_programs; p++) {
            AVProgram *program = ic->programs[p];
            if(program->id != programid)
                continue;
            for(q=0; q<program->nb_stream_indexes; q++) {
                int sidx = program->stream_index[q];
                int ris = file_table[z].ist_index + sidx;
                if(ist_table[ris]->discard && ic->streams[sidx]->codec->codec_type == type)
                    return ris;
            }
        }
    }

    return -1;
}

/*
 * The following code is the main loop of the file converter
 */
static int av_encode(AVFormatContext **output_files,
                     int nb_output_files,
                     AVFormatContext **input_files,
                     int nb_input_files,
                     AVStreamMap *stream_maps, int nb_stream_maps)
{
    int ret, i, j, k, n, nb_istreams = 0, nb_ostreams = 0;
    AVFormatContext *is, *os;
    AVCodecContext *codec, *icodec;
    AVOutputStream *ost, **ost_table = NULL;
    AVInputStream *ist, **ist_table = NULL;
    AVInputFile *file_table;
    int key;
    int want_sdp = 1;

    file_table= av_mallocz(nb_input_files * sizeof(AVInputFile));
    if (!file_table)
        goto fail;

    /* input stream init */
    j = 0;
    for(i=0;i<nb_input_files;i++) {
        is = input_files[i];
        file_table[i].ist_index = j;
        file_table[i].nb_streams = is->nb_streams;
        j += is->nb_streams;
    }
    nb_istreams = j;

    ist_table = av_mallocz(nb_istreams * sizeof(AVInputStream *));
    if (!ist_table)
        goto fail;

    for(i=0;i<nb_istreams;i++) {
        ist = av_mallocz(sizeof(AVInputStream));
        if (!ist)
            goto fail;
        ist_table[i] = ist;
    }
    j = 0;
    for(i=0;i<nb_input_files;i++) {
        is = input_files[i];
        for(k=0;k<is->nb_streams;k++) {
            ist = ist_table[j++];
            ist->st = is->streams[k];
            ist->file_index = i;
            ist->index = k;
            ist->discard = 1; /* the stream is discarded by default
                                 (changed later) */

            if (rate_emu) {
                ist->start = av_gettime();
            }
        }
    }

    /* output stream init */
    nb_ostreams = 0;
    for(i=0;i<nb_output_files;i++) {
        os = output_files[i];
        if (!os->nb_streams) {
            dump_format(output_files[i], i, output_files[i]->filename, 1);
            fprintf(stderr, "Output file #%d does not contain any stream\n", i);
            av_exit(1);
        }
        nb_ostreams += os->nb_streams;
    }
    if (nb_stream_maps > 0 && nb_stream_maps != nb_ostreams) {
        fprintf(stderr, "Number of stream maps must match number of output streams\n");
        av_exit(1);
    }

    /* Sanity check the mapping args -- do the input files & streams exist? */
    for(i=0;i<nb_stream_maps;i++) {
        int fi = stream_maps[i].file_index;
        int si = stream_maps[i].stream_index;

        if (fi < 0 || fi > nb_input_files - 1 ||
            si < 0 || si > file_table[fi].nb_streams - 1) {
            fprintf(stderr,"Could not find input stream #%d.%d\n", fi, si);
            av_exit(1);
        }
        fi = stream_maps[i].sync_file_index;
        si = stream_maps[i].sync_stream_index;
        if (fi < 0 || fi > nb_input_files - 1 ||
            si < 0 || si > file_table[fi].nb_streams - 1) {
            fprintf(stderr,"Could not find sync stream #%d.%d\n", fi, si);
            av_exit(1);
        }
    }

    ost_table = av_mallocz(sizeof(AVOutputStream *) * nb_ostreams);
    if (!ost_table)
        goto fail;
    for(i=0;i<nb_ostreams;i++) {
        ost = av_mallocz(sizeof(AVOutputStream));
        if (!ost)
            goto fail;
        ost_table[i] = ost;
    }

    n = 0;
    for(k=0;k<nb_output_files;k++) {
        os = output_files[k];
        for(i=0;i<os->nb_streams;i++,n++) {
            int found;
            ost = ost_table[n];
            ost->file_index = k;
            ost->index = i;
            ost->st = os->streams[i];
            if (nb_stream_maps > 0) {
                ost->source_index = file_table[stream_maps[n].file_index].ist_index +
                    stream_maps[n].stream_index;

                /* Sanity check that the stream types match */
                if (ist_table[ost->source_index]->st->codec->codec_type != ost->st->codec->codec_type) {
                    int i= ost->file_index;
                    dump_format(output_files[i], i, output_files[i]->filename, 1);
                    fprintf(stderr, "Codec type mismatch for mapping #%d.%d -> #%d.%d\n",
                        stream_maps[n].file_index, stream_maps[n].stream_index,
                        ost->file_index, ost->index);
                    av_exit(1);
                }

            } else {
                if(opt_programid) {
                    found = 0;
                    j = stream_index_from_inputs(input_files, nb_input_files, file_table, ist_table, ost->st->codec->codec_type, opt_programid);
                    if(j != -1) {
                        ost->source_index = j;
                        found = 1;
                    }
                } else {
                    /* get corresponding input stream index : we select the first one with the right type */
                    found = 0;
                    for(j=0;j<nb_istreams;j++) {
                        ist = ist_table[j];
                        if (ist->discard &&
                            ist->st->codec->codec_type == ost->st->codec->codec_type) {
                            ost->source_index = j;
                            found = 1;
                            break;
                        }
                    }
                }

                if (!found) {
                    if(! opt_programid) {
                        /* try again and reuse existing stream */
                        for(j=0;j<nb_istreams;j++) {
                            ist = ist_table[j];
                            if (ist->st->codec->codec_type == ost->st->codec->codec_type) {
                                ost->source_index = j;
                                found = 1;
                            }
                        }
                    }
                    if (!found) {
                        int i= ost->file_index;
                        dump_format(output_files[i], i, output_files[i]->filename, 1);
                        fprintf(stderr, "Could not find input stream matching output stream #%d.%d\n",
                                ost->file_index, ost->index);
                        av_exit(1);
                    }
                }
            }
            ist = ist_table[ost->source_index];
            ist->discard = 0;
            ost->sync_ist = (nb_stream_maps > 0) ?
                ist_table[file_table[stream_maps[n].sync_file_index].ist_index +
                         stream_maps[n].sync_stream_index] : ist;
        }
    }

    /* for each output stream, we compute the right encoding parameters */
    for(i=0;i<nb_ostreams;i++) {
        ost = ost_table[i];
        os = output_files[ost->file_index];
        ist = ist_table[ost->source_index];

        codec = ost->st->codec;
        icodec = ist->st->codec;

        if (!ost->st->language[0])
            av_strlcpy(ost->st->language, ist->st->language,
                       sizeof(ost->st->language));

        ost->st->disposition = ist->st->disposition;

        if (ost->st->stream_copy) {
            /* if stream_copy is selected, no need to decode or encode */
            codec->codec_id = icodec->codec_id;
            codec->codec_type = icodec->codec_type;

            if(!codec->codec_tag){
                if(   !os->oformat->codec_tag
                   || av_codec_get_id (os->oformat->codec_tag, icodec->codec_tag) > 0
                   || av_codec_get_tag(os->oformat->codec_tag, icodec->codec_id) <= 0)
                    codec->codec_tag = icodec->codec_tag;
            }

            codec->bit_rate = icodec->bit_rate;
            codec->extradata= icodec->extradata;
            codec->extradata_size= icodec->extradata_size;
            if(av_q2d(icodec->time_base) > av_q2d(ist->st->time_base) && av_q2d(ist->st->time_base) < 1.0/1000)
                codec->time_base = icodec->time_base;
            else
                codec->time_base = ist->st->time_base;
            switch(codec->codec_type) {
            case CODEC_TYPE_AUDIO:
                if(audio_volume != 256) {
                    fprintf(stderr,"-acodec copy and -vol are incompatible (frames are not decoded)\n");
                    av_exit(1);
                }
                codec->channel_layout = icodec->channel_layout;
                codec->sample_rate = icodec->sample_rate;
                codec->channels = icodec->channels;
                codec->frame_size = icodec->frame_size;
                codec->block_align= icodec->block_align;
                if(codec->block_align == 1 && codec->codec_id == CODEC_ID_MP3)
                    codec->block_align= 0;
                if(codec->codec_id == CODEC_ID_AC3)
                    codec->block_align= 0;
                break;
            case CODEC_TYPE_VIDEO:
                if(using_vhook) {
                    fprintf(stderr,"-vcodec copy and -vhook are incompatible (frames are not decoded)\n");
                    av_exit(1);
                }
                codec->pix_fmt = icodec->pix_fmt;
                codec->width = icodec->width;
                codec->height = icodec->height;
                codec->has_b_frames = icodec->has_b_frames;
                break;
            case CODEC_TYPE_SUBTITLE:
                break;
            default:
                abort();
            }
        } else {
            switch(codec->codec_type) {
            case CODEC_TYPE_AUDIO:
                if (av_fifo_init(&ost->fifo, 1024))
                    goto fail;
                ost->reformat_pair = MAKE_SFMT_PAIR(SAMPLE_FMT_NONE,SAMPLE_FMT_NONE);
                ost->audio_resample = codec->sample_rate != icodec->sample_rate || audio_sync_method > 1;
                icodec->request_channels = codec->channels;
                ist->decoding_needed = 1;
                ost->encoding_needed = 1;
                break;
            case CODEC_TYPE_VIDEO:
                ost->video_crop = ((frame_leftBand + frame_rightBand + frame_topBand + frame_bottomBand) != 0);
                ost->video_pad = ((frame_padleft + frame_padright + frame_padtop + frame_padbottom) != 0);
                ost->video_resample = ((codec->width != icodec->width -
                                (frame_leftBand + frame_rightBand) +
                                (frame_padleft + frame_padright)) ||
                        (codec->height != icodec->height -
                                (frame_topBand  + frame_bottomBand) +
                                (frame_padtop + frame_padbottom)) ||
                        (codec->pix_fmt != icodec->pix_fmt));
                if (ost->video_crop) {
                    ost->topBand = frame_topBand;
                    ost->leftBand = frame_leftBand;
                }
                if (ost->video_pad) {
                    ost->padtop = frame_padtop;
                    ost->padleft = frame_padleft;
                    ost->padbottom = frame_padbottom;
                    ost->padright = frame_padright;
                    if (!ost->video_resample) {
                        avcodec_get_frame_defaults(&ost->pict_tmp);
                        if(avpicture_alloc((AVPicture*)&ost->pict_tmp, codec->pix_fmt,
                                         codec->width, codec->height))
                            goto fail;
                    }
                }
                if (ost->video_resample) {
                    avcodec_get_frame_defaults(&ost->pict_tmp);
                    if(avpicture_alloc((AVPicture*)&ost->pict_tmp, codec->pix_fmt,
                                         codec->width, codec->height)) {
                        fprintf(stderr, "Cannot allocate temp picture, check pix fmt\n");
                        av_exit(1);
                    }
                    sws_flags = av_get_int(sws_opts, "sws_flags", NULL);
                    ost->img_resample_ctx = sws_getContext(
                            icodec->width - (frame_leftBand + frame_rightBand),
                            icodec->height - (frame_topBand + frame_bottomBand),
                            icodec->pix_fmt,
                            codec->width - (frame_padleft + frame_padright),
                            codec->height - (frame_padtop + frame_padbottom),
                            codec->pix_fmt,
                            sws_flags, NULL, NULL, NULL);
                    if (ost->img_resample_ctx == NULL) {
                        fprintf(stderr, "Cannot get resampling context\n");
                        av_exit(1);
                    }
                    ost->resample_height = icodec->height - (frame_topBand + frame_bottomBand);
                }
                ost->encoding_needed = 1;
                ist->decoding_needed = 1;
                break;
            case CODEC_TYPE_SUBTITLE:
                ost->encoding_needed = 1;
                ist->decoding_needed = 1;
                break;
            default:
                abort();
                break;
            }
            /* two pass mode */
            if (ost->encoding_needed &&
                (codec->flags & (CODEC_FLAG_PASS1 | CODEC_FLAG_PASS2))) {
                char logfilename[1024];
                FILE *f;
                int size;
                char *logbuffer;

                snprintf(logfilename, sizeof(logfilename), "%s-%d.log",
                         pass_logfilename ?
                         pass_logfilename : DEFAULT_PASS_LOGFILENAME, i);
                if (codec->flags & CODEC_FLAG_PASS1) {
                    f = fopen(logfilename, "w");
                    if (!f) {
                        fprintf(stderr, "Cannot write log file '%s' for pass-1 encoding: %s\n", logfilename, strerror(errno));
                        av_exit(1);
                    }
                    ost->logfile = f;
                } else {
                    /* read the log file */
                    f = fopen(logfilename, "r");
                    if (!f) {
                        fprintf(stderr, "Cannot read log file '%s' for pass-2 encoding: %s\n", logfilename, strerror(errno));
                        av_exit(1);
                    }
                    fseek(f, 0, SEEK_END);
                    size = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    logbuffer = av_malloc(size + 1);
                    if (!logbuffer) {
                        fprintf(stderr, "Could not allocate log buffer\n");
                        av_exit(1);
                    }
                    size = fread(logbuffer, 1, size, f);
                    fclose(f);
                    logbuffer[size] = '\0';
                    codec->stats_in = logbuffer;
                }
            }
        }
        if(codec->codec_type == CODEC_TYPE_VIDEO){
            int size= codec->width * codec->height;
            bit_buffer_size= FFMAX(bit_buffer_size, 4*size);
        }
    }

    if (!bit_buffer)
        bit_buffer = av_malloc(bit_buffer_size);
    if (!bit_buffer)
        goto fail;

    /* dump the file output parameters - cannot be done before in case
       of stream copy */
    for(i=0;i<nb_output_files;i++) {
        dump_format(output_files[i], i, output_files[i]->filename, 1);
    }

    /* dump the stream mapping */
    if (verbose >= 0) {
        fprintf(stderr, "Stream mapping:\n");
        for(i=0;i<nb_ostreams;i++) {
            ost = ost_table[i];
            fprintf(stderr, "  Stream #%d.%d -> #%d.%d",
                    ist_table[ost->source_index]->file_index,
                    ist_table[ost->source_index]->index,
                    ost->file_index,
                    ost->index);
            if (ost->sync_ist != ist_table[ost->source_index])
                fprintf(stderr, " [sync #%d.%d]",
                        ost->sync_ist->file_index,
                        ost->sync_ist->index);
            fprintf(stderr, "\n");
        }
    }

    /* open each encoder */
    for(i=0;i<nb_ostreams;i++) {
        ost = ost_table[i];
        if (ost->encoding_needed) {
            AVCodec *codec = output_codecs[i];
            if (!codec)
                codec = avcodec_find_encoder(ost->st->codec->codec_id);
            if (!codec) {
                fprintf(stderr, "Unsupported codec for output stream #%d.%d\n",
                        ost->file_index, ost->index);
                av_exit(1);
            }
            if (avcodec_open(ost->st->codec, codec) < 0) {
                fprintf(stderr, "Error while opening codec for output stream #%d.%d - maybe incorrect parameters such as bit_rate, rate, width or height\n",
                        ost->file_index, ost->index);
                av_exit(1);
            }
            extra_size += ost->st->codec->extradata_size;
        }
    }

    /* open each decoder */
    for(i=0;i<nb_istreams;i++) {
        ist = ist_table[i];
        if (ist->decoding_needed) {
            AVCodec *codec = input_codecs[i];
            if (!codec)
                codec = avcodec_find_decoder(ist->st->codec->codec_id);
            if (!codec) {
                fprintf(stderr, "Unsupported codec (id=%d) for input stream #%d.%d\n",
                        ist->st->codec->codec_id, ist->file_index, ist->index);
                av_exit(1);
            }
            if (avcodec_open(ist->st->codec, codec) < 0) {
                fprintf(stderr, "Error while opening codec for input stream #%d.%d\n",
                        ist->file_index, ist->index);
                av_exit(1);
            }
            //if (ist->st->codec->codec_type == CODEC_TYPE_VIDEO)
            //    ist->st->codec->flags |= CODEC_FLAG_REPEAT_FIELD;
        }
    }

    /* init pts */
    for(i=0;i<nb_istreams;i++) {
        ist = ist_table[i];
        is = input_files[ist->file_index];
        ist->pts = 0;
        ist->next_pts = AV_NOPTS_VALUE;
        ist->is_start = 1;
    }

    /* set meta data information from input file if required */
    for (i=0;i<nb_meta_data_maps;i++) {
        AVFormatContext *out_file;
        AVFormatContext *in_file;

        int out_file_index = meta_data_maps[i].out_file;
        int in_file_index = meta_data_maps[i].in_file;
        if (out_file_index < 0 || out_file_index >= nb_output_files) {
            fprintf(stderr, "Invalid output file index %d map_meta_data(%d,%d)\n", out_file_index, out_file_index, in_file_index);
            ret = AVERROR(EINVAL);
            goto fail;
        }
        if (in_file_index < 0 || in_file_index >= nb_input_files) {
            fprintf(stderr, "Invalid input file index %d map_meta_data(%d,%d)\n", in_file_index, out_file_index, in_file_index);
            ret = AVERROR(EINVAL);
            goto fail;
        }

        out_file = output_files[out_file_index];
        in_file = input_files[in_file_index];

        strcpy(out_file->title, in_file->title);
        strcpy(out_file->author, in_file->author);
        strcpy(out_file->copyright, in_file->copyright);
        strcpy(out_file->comment, in_file->comment);
        strcpy(out_file->album, in_file->album);
        out_file->year = in_file->year;
        out_file->track = in_file->track;
        strcpy(out_file->genre, in_file->genre);
    }

    /* open files and write file headers */
    for(i=0;i<nb_output_files;i++) {
        os = output_files[i];
        if (av_write_header(os) < 0) {
            fprintf(stderr, "Could not write header for output file #%d (incorrect codec parameters ?)\n", i);
            ret = AVERROR(EINVAL);
            goto fail;
        }
        if (strcmp(output_files[i]->oformat->name, "rtp")) {
            want_sdp = 0;
        }
    }
    if (want_sdp) {
        print_sdp(output_files, nb_output_files);
    }

    if (!using_stdin && verbose >= 0) {
        fprintf(stderr, "Press [q] to stop encoding\n");
        url_set_interrupt_cb(decode_interrupt_cb);
    }
    term_init();

    key = -1;
    timer_start = av_gettime();

    for(; received_sigterm == 0;) {
        int file_index, ist_index;
        AVPacket pkt;
        double ipts_min;
        double opts_min;

    redo:
        ipts_min= 1e100;
        opts_min= 1e100;
        /* if 'q' pressed, exits */
        if (!using_stdin) {
            if (q_pressed)
                break;
            /* read_key() returns 0 on EOF */
            key = read_key();
            if (key == 'q')
                break;
        }

        /* select the stream that we must read now by looking at the
           smallest output pts */
        file_index = -1;
        for(i=0;i<nb_ostreams;i++) {
            double ipts, opts;
            ost = ost_table[i];
            os = output_files[ost->file_index];
            ist = ist_table[ost->source_index];
            if(ost->st->codec->codec_type == CODEC_TYPE_VIDEO)
                opts = ost->sync_opts * av_q2d(ost->st->codec->time_base);
            else
                opts = ost->st->pts.val * av_q2d(ost->st->time_base);
            ipts = (double)ist->pts;
            if (!file_table[ist->file_index].eof_reached){
                if(ipts < ipts_min) {
                    ipts_min = ipts;
                    if(input_sync ) file_index = ist->file_index;
                }
                if(opts < opts_min) {
                    opts_min = opts;
                    if(!input_sync) file_index = ist->file_index;
                }
            }
            if(ost->frame_number >= max_frames[ost->st->codec->codec_type]){
                file_index= -1;
                break;
            }
        }
        /* if none, if is finished */
        if (file_index < 0) {
            break;
        }

        /* finish if recording time exhausted */
        if (opts_min >= (recording_time / 1000000.0))
            break;

        /* finish if limit size exhausted */
        if (limit_filesize != 0 && limit_filesize < url_ftell(output_files[0]->pb))
            break;

        /* read a frame from it and output it in the fifo */
        is = input_files[file_index];
        if (av_read_frame(is, &pkt) < 0) {
            file_table[file_index].eof_reached = 1;
            if (opt_shortest)
                break;
            else
                continue;
        }

        if (do_pkt_dump) {
            av_pkt_dump_log(NULL, AV_LOG_DEBUG, &pkt, do_hex_dump);
        }
        /* the following test is needed in case new streams appear
           dynamically in stream : we ignore them */
        if (pkt.stream_index >= file_table[file_index].nb_streams)
            goto discard_packet;
        ist_index = file_table[file_index].ist_index + pkt.stream_index;
        ist = ist_table[ist_index];
        if (ist->discard)
            goto discard_packet;

        if (pkt.dts != AV_NOPTS_VALUE)
            pkt.dts += av_rescale_q(input_files_ts_offset[ist->file_index], AV_TIME_BASE_Q, ist->st->time_base);
        if (pkt.pts != AV_NOPTS_VALUE)
            pkt.pts += av_rescale_q(input_files_ts_offset[ist->file_index], AV_TIME_BASE_Q, ist->st->time_base);

        if(input_files_ts_scale[file_index][pkt.stream_index]){
            if(pkt.pts != AV_NOPTS_VALUE)
                pkt.pts *= input_files_ts_scale[file_index][pkt.stream_index];
            if(pkt.dts != AV_NOPTS_VALUE)
                pkt.dts *= input_files_ts_scale[file_index][pkt.stream_index];
        }

//        fprintf(stderr, "next:%"PRId64" dts:%"PRId64" off:%"PRId64" %d\n", ist->next_pts, pkt.dts, input_files_ts_offset[ist->file_index], ist->st->codec->codec_type);
        if (pkt.dts != AV_NOPTS_VALUE && ist->next_pts != AV_NOPTS_VALUE
            && (is->iformat->flags & AVFMT_TS_DISCONT)) {
            int64_t pkt_dts= av_rescale_q(pkt.dts, ist->st->time_base, AV_TIME_BASE_Q);
            int64_t delta= pkt_dts - ist->next_pts;
            if((FFABS(delta) > 1LL*dts_delta_threshold*AV_TIME_BASE || pkt_dts+1<ist->pts)&& !copy_ts){
                input_files_ts_offset[ist->file_index]-= delta;
                if (verbose > 2)
                    fprintf(stderr, "timestamp discontinuity %"PRId64", new offset= %"PRId64"\n", delta, input_files_ts_offset[ist->file_index]);
                pkt.dts-= av_rescale_q(delta, AV_TIME_BASE_Q, ist->st->time_base);
                if(pkt.pts != AV_NOPTS_VALUE)
                    pkt.pts-= av_rescale_q(delta, AV_TIME_BASE_Q, ist->st->time_base);
            }
        }

        //fprintf(stderr,"read #%d.%d size=%d\n", ist->file_index, ist->index, pkt.size);
        if (output_packet(ist, ist_index, ost_table, nb_ostreams, &pkt) < 0) {

            if (verbose >= 0)
                fprintf(stderr, "Error while decoding stream #%d.%d\n",
                        ist->file_index, ist->index);
            if (exit_on_error)
                av_exit(1);
            av_free_packet(&pkt);
            goto redo;
        }

    discard_packet:
        av_free_packet(&pkt);

        /* dump report by using the output first video and audio streams */
        print_report(output_files, ost_table, nb_ostreams, 0);
    }

    /* at the end of stream, we must flush the decoder buffers */
    for(i=0;i<nb_istreams;i++) {
        ist = ist_table[i];
        if (ist->decoding_needed) {
            output_packet(ist, i, ost_table, nb_ostreams, NULL);
        }
    }

    term_exit();

    /* write the trailer if needed and close file */
    for(i=0;i<nb_output_files;i++) {
        os = output_files[i];
        av_write_trailer(os);
    }

    /* dump report by using the first video and audio streams */
    print_report(output_files, ost_table, nb_ostreams, 1);

    /* close each encoder */
    for(i=0;i<nb_ostreams;i++) {
        ost = ost_table[i];
        if (ost->encoding_needed) {
            av_freep(&ost->st->codec->stats_in);
            avcodec_close(ost->st->codec);
        }
    }

    /* close each decoder */
    for(i=0;i<nb_istreams;i++) {
        ist = ist_table[i];
        if (ist->decoding_needed) {
            avcodec_close(ist->st->codec);
        }
    }

    /* finished ! */

    ret = 0;
 fail1:
    av_freep(&bit_buffer);
    av_free(file_table);

    if (ist_table) {
        for(i=0;i<nb_istreams;i++) {
            ist = ist_table[i];
            av_free(ist);
        }
        av_free(ist_table);
    }
    if (ost_table) {
        for(i=0;i<nb_ostreams;i++) {
            ost = ost_table[i];
            if (ost) {
                if (ost->logfile) {
                    fclose(ost->logfile);
                    ost->logfile = NULL;
                }
                av_fifo_free(&ost->fifo); /* works even if fifo is not
                                             initialized but set to zero */
                av_free(ost->pict_tmp.data[0]);
                if (ost->video_resample)
                    sws_freeContext(ost->img_resample_ctx);
                if (ost->resample)
                    audio_resample_close(ost->resample);
                if (ost->reformat_ctx)
                    av_audio_convert_free(ost->reformat_ctx);
                av_free(ost);
            }
        }
        av_free(ost_table);
    }
    return ret;
 fail:
    ret = AVERROR(ENOMEM);
    goto fail1;
}

#if 0
int file_read(const char *filename)
{
    URLContext *h;
    unsigned char buffer[1024];
    int len, i;

    if (url_open(&h, filename, O_RDONLY) < 0) {
        printf("could not open '%s'\n", filename);
        return -1;
    }
    for(;;) {
        len = url_read(h, buffer, sizeof(buffer));
        if (len <= 0)
            break;
        for(i=0;i<len;i++) putchar(buffer[i]);
    }
    url_close(h);
    return 0;
}
#endif

static void opt_format(const char *arg)
{
    /* compatibility stuff for pgmyuv */
    if (!strcmp(arg, "pgmyuv")) {
        pgmyuv_compatibility_hack=1;
//        opt_image_format(arg);
        arg = "image2";
        fprintf(stderr, "pgmyuv format is deprecated, use image2\n");
    }

    file_iformat = av_find_input_format(arg);
    file_oformat = guess_format(arg, NULL, NULL);
    if (!file_iformat && !file_oformat) {
        fprintf(stderr, "Unknown input or output format: %s\n", arg);
        av_exit(1);
    }
}

static void opt_video_rc_override_string(const char *arg)
{
    video_rc_override_string = arg;
}

static int opt_me_threshold(const char *opt, const char *arg)
{
    me_threshold = parse_number_or_die(opt, arg, OPT_INT64, INT_MIN, INT_MAX);
    return 0;
}

static int opt_verbose(const char *opt, const char *arg)
{
    verbose = parse_number_or_die(opt, arg, OPT_INT64, -10, 10);
    av_log_set_level(verbose);
    return 0;
}

static int opt_frame_rate(const char *opt, const char *arg)
{
    if (av_parse_video_frame_rate(&frame_rate, arg) < 0) {
        fprintf(stderr, "Incorrect value for %s: %s\n", opt, arg);
        av_exit(1);
    }
    return 0;
}

static int opt_bitrate(const char *opt, const char *arg)
{
    int codec_type = opt[0]=='a' ? CODEC_TYPE_AUDIO : CODEC_TYPE_VIDEO;

    opt_default(opt, arg);

    if (av_get_int(avctx_opts[codec_type], "b", NULL) < 1000)
        fprintf(stderr, "WARNING: The bitrate parameter is set too low. It takes bits/s as argument, not kbits/s\n");

    return 0;
}

static void opt_frame_crop_top(const char *arg)
{
    frame_topBand = atoi(arg);
    if (frame_topBand < 0) {
        fprintf(stderr, "Incorrect top crop size\n");
        av_exit(1);
    }
    if ((frame_topBand % 2) != 0) {
        fprintf(stderr, "Top crop size must be a multiple of 2\n");
        av_exit(1);
    }
    if ((frame_topBand) >= frame_height){
        fprintf(stderr, "Vertical crop dimensions are outside the range of the original image.\nRemember to crop first and scale second.\n");
        av_exit(1);
    }
    frame_height -= frame_topBand;
}

static void opt_frame_crop_bottom(const char *arg)
{
    frame_bottomBand = atoi(arg);
    if (frame_bottomBand < 0) {
        fprintf(stderr, "Incorrect bottom crop size\n");
        av_exit(1);
    }
    if ((frame_bottomBand % 2) != 0) {
        fprintf(stderr, "Bottom crop size must be a multiple of 2\n");
        av_exit(1);
    }
    if ((frame_bottomBand) >= frame_height){
        fprintf(stderr, "Vertical crop dimensions are outside the range of the original image.\nRemember to crop first and scale second.\n");
        av_exit(1);
    }
    frame_height -= frame_bottomBand;
}

static void opt_frame_crop_left(const char *arg)
{
    frame_leftBand = atoi(arg);
    if (frame_leftBand < 0) {
        fprintf(stderr, "Incorrect left crop size\n");
        av_exit(1);
    }
    if ((frame_leftBand % 2) != 0) {
        fprintf(stderr, "Left crop size must be a multiple of 2\n");
        av_exit(1);
    }
    if ((frame_leftBand) >= frame_width){
        fprintf(stderr, "Horizontal crop dimensions are outside the range of the original image.\nRemember to crop first and scale second.\n");
        av_exit(1);
    }
    frame_width -= frame_leftBand;
}

static void opt_frame_crop_right(const char *arg)
{
    frame_rightBand = atoi(arg);
    if (frame_rightBand < 0) {
        fprintf(stderr, "Incorrect right crop size\n");
        av_exit(1);
    }
    if ((frame_rightBand % 2) != 0) {
        fprintf(stderr, "Right crop size must be a multiple of 2\n");
        av_exit(1);
    }
    if ((frame_rightBand) >= frame_width){
        fprintf(stderr, "Horizontal crop dimensions are outside the range of the original image.\nRemember to crop first and scale second.\n");
        av_exit(1);
    }
    frame_width -= frame_rightBand;
}

static void opt_frame_size(const char *arg)
{
    if (av_parse_video_frame_size(&frame_width, &frame_height, arg) < 0) {
        fprintf(stderr, "Incorrect frame size\n");
        av_exit(1);
    }
    if ((frame_width % 2) != 0 || (frame_height % 2) != 0) {
        fprintf(stderr, "Frame size must be a multiple of 2\n");
        av_exit(1);
    }
}


#define SCALEBITS 10
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))

#define RGB_TO_Y(r, g, b) \
((FIX(0.29900) * (r) + FIX(0.58700) * (g) + \
  FIX(0.11400) * (b) + ONE_HALF) >> SCALEBITS)

#define RGB_TO_U(r1, g1, b1, shift)\
(((- FIX(0.16874) * r1 - FIX(0.33126) * g1 +         \
     FIX(0.50000) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_V(r1, g1, b1, shift)\
(((FIX(0.50000) * r1 - FIX(0.41869) * g1 -           \
   FIX(0.08131) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

static void opt_pad_color(const char *arg) {
    /* Input is expected to be six hex digits similar to
       how colors are expressed in html tags (but without the #) */
    int rgb = strtol(arg, NULL, 16);
    int r,g,b;

    r = (rgb >> 16);
    g = ((rgb >> 8) & 255);
    b = (rgb & 255);

    padcolor[0] = RGB_TO_Y(r,g,b);
    padcolor[1] = RGB_TO_U(r,g,b,0);
    padcolor[2] = RGB_TO_V(r,g,b,0);
}

static void opt_frame_pad_top(const char *arg)
{
    frame_padtop = atoi(arg);
    if (frame_padtop < 0) {
        fprintf(stderr, "Incorrect top pad size\n");
        av_exit(1);
    }
    if ((frame_padtop % 2) != 0) {
        fprintf(stderr, "Top pad size must be a multiple of 2\n");
        av_exit(1);
    }
}

static void opt_frame_pad_bottom(const char *arg)
{
    frame_padbottom = atoi(arg);
    if (frame_padbottom < 0) {
        fprintf(stderr, "Incorrect bottom pad size\n");
        av_exit(1);
    }
    if ((frame_padbottom % 2) != 0) {
        fprintf(stderr, "Bottom pad size must be a multiple of 2\n");
        av_exit(1);
    }
}


static void opt_frame_pad_left(const char *arg)
{
    frame_padleft = atoi(arg);
    if (frame_padleft < 0) {
        fprintf(stderr, "Incorrect left pad size\n");
        av_exit(1);
    }
    if ((frame_padleft % 2) != 0) {
        fprintf(stderr, "Left pad size must be a multiple of 2\n");
        av_exit(1);
    }
}


static void opt_frame_pad_right(const char *arg)
{
    frame_padright = atoi(arg);
    if (frame_padright < 0) {
        fprintf(stderr, "Incorrect right pad size\n");
        av_exit(1);
    }
    if ((frame_padright % 2) != 0) {
        fprintf(stderr, "Right pad size must be a multiple of 2\n");
        av_exit(1);
    }
}

static void list_fmts(void (*get_fmt_string)(char *buf, int buf_size, int fmt), int nb_fmts)
{
    int i;
    char fmt_str[128];
    for (i=-1; i < nb_fmts; i++) {
        get_fmt_string (fmt_str, sizeof(fmt_str), i);
        fprintf(stdout, "%s\n", fmt_str);
    }
}

static void opt_frame_pix_fmt(const char *arg)
{
    if (strcmp(arg, "list"))
        frame_pix_fmt = avcodec_get_pix_fmt(arg);
    else {
        list_fmts(avcodec_pix_fmt_string, PIX_FMT_NB);
        av_exit(0);
    }
}

static void opt_frame_aspect_ratio(const char *arg)
{
    int x = 0, y = 0;
    double ar = 0;
    const char *p;
    char *end;

    p = strchr(arg, ':');
    if (p) {
        x = strtol(arg, &end, 10);
        if (end == p)
            y = strtol(end+1, &end, 10);
        if (x > 0 && y > 0)
            ar = (double)x / (double)y;
    } else
        ar = strtod(arg, NULL);

    if (!ar) {
        fprintf(stderr, "Incorrect aspect ratio specification.\n");
        av_exit(1);
    }
    frame_aspect_ratio = ar;
}

static void opt_qscale(const char *arg)
{
    video_qscale = atof(arg);
    if (video_qscale <= 0 ||
        video_qscale > 255) {
        fprintf(stderr, "qscale must be > 0.0 and <= 255\n");
        av_exit(1);
    }
}

static void opt_top_field_first(const char *arg)
{
    top_field_first= atoi(arg);
}

static int opt_thread_count(const char *opt, const char *arg)
{
    thread_count= parse_number_or_die(opt, arg, OPT_INT64, 0, INT_MAX);
#if !defined(HAVE_THREADS)
    if (verbose >= 0)
        fprintf(stderr, "Warning: not compiled with thread support, using thread emulation\n");
#endif
    return 0;
}

static void opt_audio_sample_fmt(const char *arg)
{
    if (strcmp(arg, "list"))
        audio_sample_fmt = avcodec_get_sample_fmt(arg);
    else {
        list_fmts(avcodec_sample_fmt_string, SAMPLE_FMT_NB);
        av_exit(0);
    }
}

static int opt_audio_rate(const char *opt, const char *arg)
{
    audio_sample_rate = parse_number_or_die(opt, arg, OPT_INT64, 0, INT_MAX);
    return 0;
}

static int opt_audio_channels(const char *opt, const char *arg)
{
    audio_channels = parse_number_or_die(opt, arg, OPT_INT64, 0, INT_MAX);
    return 0;
}

static void opt_video_channel(const char *arg)
{
    video_channel = strtol(arg, NULL, 0);
}

static void opt_video_standard(const char *arg)
{
    video_standard = av_strdup(arg);
}

static void opt_codec(int *pstream_copy, char **pcodec_name,
                      int codec_type, const char *arg)
{
    av_freep(pcodec_name);
    if (!strcmp(arg, "copy")) {
        *pstream_copy = 1;
    } else {
        *pcodec_name = av_strdup(arg);
    }
}

static void opt_audio_codec(const char *arg)
{
    opt_codec(&audio_stream_copy, &audio_codec_name, CODEC_TYPE_AUDIO, arg);
}

static void opt_audio_tag(const char *arg)
{
    char *tail;
    audio_codec_tag= strtol(arg, &tail, 0);

    if(!tail || *tail)
        audio_codec_tag= arg[0] + (arg[1]<<8) + (arg[2]<<16) + (arg[3]<<24);
}

static void opt_video_tag(const char *arg)
{
    char *tail;
    video_codec_tag= strtol(arg, &tail, 0);

    if(!tail || *tail)
        video_codec_tag= arg[0] + (arg[1]<<8) + (arg[2]<<16) + (arg[3]<<24);
}

#ifdef CONFIG_VHOOK
static void add_frame_hooker(const char *arg)
{
    int argc = 0;
    char *argv[64];
    int i;
    char *args = av_strdup(arg);

    using_vhook = 1;

    argv[0] = strtok(args, " ");
    while (argc < 62 && (argv[++argc] = strtok(NULL, " "))) {
    }

    i = frame_hook_add(argc, argv);

    if (i != 0) {
        fprintf(stderr, "Failed to add video hook function: %s\n", arg);
        av_exit(1);
    }
}
#endif

static void opt_video_codec(const char *arg)
{
    opt_codec(&video_stream_copy, &video_codec_name, CODEC_TYPE_VIDEO, arg);
}

static void opt_subtitle_codec(const char *arg)
{
    opt_codec(&subtitle_stream_copy, &subtitle_codec_name, CODEC_TYPE_SUBTITLE, arg);
}

static void opt_map(const char *arg)
{
    AVStreamMap *m;
    char *p;

    m = &stream_maps[nb_stream_maps++];

    m->file_index = strtol(arg, &p, 0);
    if (*p)
        p++;

    m->stream_index = strtol(p, &p, 0);
    if (*p) {
        p++;
        m->sync_file_index = strtol(p, &p, 0);
        if (*p)
            p++;
        m->sync_stream_index = strtol(p, &p, 0);
    } else {
        m->sync_file_index = m->file_index;
        m->sync_stream_index = m->stream_index;
    }
}

static void opt_map_meta_data(const char *arg)
{
    AVMetaDataMap *m;
    char *p;

    m = &meta_data_maps[nb_meta_data_maps++];

    m->out_file = strtol(arg, &p, 0);
    if (*p)
        p++;

    m->in_file = strtol(p, &p, 0);
}

static void opt_input_ts_scale(const char *arg)
{
    unsigned int stream;
    double scale;
    char *p;

    stream = strtol(arg, &p, 0);
    if (*p)
        p++;
    scale= strtod(p, &p);

    if(stream >= MAX_STREAMS)
        av_exit(1);

    input_files_ts_scale[nb_input_files][stream]= scale;
}

static int opt_recording_time(const char *opt, const char *arg)
{
    recording_time = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_start_time(const char *opt, const char *arg)
{
    start_time = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_rec_timestamp(const char *opt, const char *arg)
{
    rec_timestamp = parse_time_or_die(opt, arg, 0) / 1000000;
    return 0;
}

static int opt_input_ts_offset(const char *opt, const char *arg)
{
    input_ts_offset = parse_time_or_die(opt, arg, 1);
    return 0;
}

static enum CodecID find_codec_or_die(const char *name, int type, int encoder)
{
    const char *codec_string = encoder ? "encoder" : "decoder";
    AVCodec *codec;

    if(!name)
        return CODEC_ID_NONE;
    codec = encoder ?
        avcodec_find_encoder_by_name(name) :
        avcodec_find_decoder_by_name(name);
    if(!codec) {
        av_log(NULL, AV_LOG_ERROR, "Unknown %s '%s'\n", codec_string, name);
        av_exit(1);
    }
    if(codec->type != type) {
        av_log(NULL, AV_LOG_ERROR, "Invalid %s type '%s'\n", codec_string, name);
        av_exit(1);
    }
    return codec->id;
}

static void opt_input_file(const char *filename)
{
    AVFormatContext *ic;
    AVFormatParameters params, *ap = &params;
    int err, i, ret, rfps, rfps_base;
    int64_t timestamp;

    if (!strcmp(filename, "-"))
        filename = "pipe:";

    using_stdin |= !strncmp(filename, "pipe:", 5) ||
                    !strcmp(filename, "/dev/stdin");

    /* get default parameters from command line */
    ic = av_alloc_format_context();

    memset(ap, 0, sizeof(*ap));
    ap->prealloced_context = 1;
    ap->sample_rate = audio_sample_rate;
    ap->channels = audio_channels;
    ap->time_base.den = frame_rate.num;
    ap->time_base.num = frame_rate.den;
    ap->width = frame_width + frame_padleft + frame_padright;
    ap->height = frame_height + frame_padtop + frame_padbottom;
    ap->pix_fmt = frame_pix_fmt;
   // ap->sample_fmt = audio_sample_fmt; //FIXME:not implemented in libavformat
    ap->channel = video_channel;
    ap->standard = video_standard;
    ap->video_codec_id = find_codec_or_die(video_codec_name, CODEC_TYPE_VIDEO, 0);
    ap->audio_codec_id = find_codec_or_die(audio_codec_name, CODEC_TYPE_AUDIO, 0);
    if(pgmyuv_compatibility_hack)
        ap->video_codec_id= CODEC_ID_PGMYUV;

    set_context_opts(ic, avformat_opts, AV_OPT_FLAG_DECODING_PARAM);

    ic->video_codec_id   = find_codec_or_die(video_codec_name   , CODEC_TYPE_VIDEO   , 0);
    ic->audio_codec_id   = find_codec_or_die(audio_codec_name   , CODEC_TYPE_AUDIO   , 0);
    ic->subtitle_codec_id= find_codec_or_die(subtitle_codec_name, CODEC_TYPE_SUBTITLE, 0);

    /* open the input file with generic libav function */
    err = av_open_input_file(&ic, filename, file_iformat, 0, ap);
    if (err < 0) {
        print_error(filename, err);
        av_exit(1);
    }
    if(opt_programid) {
        int i;
        for(i=0; i<ic->nb_programs; i++)
            if(ic->programs[i]->id != opt_programid)
                ic->programs[i]->discard = AVDISCARD_ALL;
    }

    ic->loop_input = loop_input;

    /* If not enough info to get the stream parameters, we decode the
       first frames to get it. (used in mpeg case for example) */
    ret = av_find_stream_info(ic);
    if (ret < 0 && verbose >= 0) {
        fprintf(stderr, "%s: could not find codec parameters\n", filename);
        av_exit(1);
    }

    timestamp = start_time;
    /* add the stream start time */
    if (ic->start_time != AV_NOPTS_VALUE)
        timestamp += ic->start_time;

    /* if seeking requested, we execute it */
    if (start_time != 0) {
        ret = av_seek_frame(ic, -1, timestamp, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            fprintf(stderr, "%s: could not seek to position %0.3f\n",
                    filename, (double)timestamp / AV_TIME_BASE);
        }
        /* reset seek info */
        start_time = 0;
    }

    /* update the current parameters so that they match the one of the input stream */
    for(i=0;i<ic->nb_streams;i++) {
        AVCodecContext *enc = ic->streams[i]->codec;
        if(thread_count>1)
            avcodec_thread_init(enc, thread_count);
        enc->thread_count= thread_count;
        switch(enc->codec_type) {
        case CODEC_TYPE_AUDIO:
            set_context_opts(enc, avctx_opts[CODEC_TYPE_AUDIO], AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_DECODING_PARAM);
            //fprintf(stderr, "\nInput Audio channels: %d", enc->channels);
            channel_layout = enc->channel_layout;
            audio_channels = enc->channels;
            audio_sample_rate = enc->sample_rate;
            audio_sample_fmt = enc->sample_fmt;
            input_codecs[nb_icodecs++] = avcodec_find_decoder_by_name(audio_codec_name);
            if(audio_disable)
                ic->streams[i]->discard= AVDISCARD_ALL;
            break;
        case CODEC_TYPE_VIDEO:
            set_context_opts(enc, avctx_opts[CODEC_TYPE_VIDEO], AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_DECODING_PARAM);
            frame_height = enc->height;
            frame_width = enc->width;
            if(ic->streams[i]->sample_aspect_ratio.num)
                frame_aspect_ratio=av_q2d(ic->streams[i]->sample_aspect_ratio);
            else
                frame_aspect_ratio=av_q2d(enc->sample_aspect_ratio);
            frame_aspect_ratio *= (float) enc->width / enc->height;
            frame_pix_fmt = enc->pix_fmt;
            rfps      = ic->streams[i]->r_frame_rate.num;
            rfps_base = ic->streams[i]->r_frame_rate.den;
            if(enc->lowres) enc->flags |= CODEC_FLAG_EMU_EDGE;
            if(me_threshold)
                enc->debug |= FF_DEBUG_MV;

            if (enc->time_base.den != rfps || enc->time_base.num != rfps_base) {

                if (verbose >= 0)
                    fprintf(stderr,"\nSeems stream %d codec frame rate differs from container frame rate: %2.2f (%d/%d) -> %2.2f (%d/%d)\n",
                            i, (float)enc->time_base.den / enc->time_base.num, enc->time_base.den, enc->time_base.num,

                    (float)rfps / rfps_base, rfps, rfps_base);
            }
            /* update the current frame rate to match the stream frame rate */
            frame_rate.num = rfps;
            frame_rate.den = rfps_base;

            input_codecs[nb_icodecs++] = avcodec_find_decoder_by_name(video_codec_name);
            if(video_disable)
                ic->streams[i]->discard= AVDISCARD_ALL;
            else if(video_discard)
                ic->streams[i]->discard= video_discard;
            break;
        case CODEC_TYPE_DATA:
            break;
        case CODEC_TYPE_SUBTITLE:
            input_codecs[nb_icodecs++] = avcodec_find_decoder_by_name(subtitle_codec_name);
            if(subtitle_disable)
                ic->streams[i]->discard = AVDISCARD_ALL;
            break;
        case CODEC_TYPE_ATTACHMENT:
        case CODEC_TYPE_UNKNOWN:
            nb_icodecs++;
            break;
        default:
            abort();
        }
    }

    input_files[nb_input_files] = ic;
    input_files_ts_offset[nb_input_files] = input_ts_offset - (copy_ts ? 0 : timestamp);
    /* dump the file content */
    if (verbose >= 0)
        dump_format(ic, nb_input_files, filename, 0);

    nb_input_files++;
    file_iformat = NULL;
    file_oformat = NULL;

    video_channel = 0;

    av_freep(&video_codec_name);
    av_freep(&audio_codec_name);
    av_freep(&subtitle_codec_name);
}

static void check_audio_video_sub_inputs(int *has_video_ptr, int *has_audio_ptr,
                                         int *has_subtitle_ptr)
{
    int has_video, has_audio, has_subtitle, i, j;
    AVFormatContext *ic;

    has_video = 0;
    has_audio = 0;
    has_subtitle = 0;
    for(j=0;j<nb_input_files;j++) {
        ic = input_files[j];
        for(i=0;i<ic->nb_streams;i++) {
            AVCodecContext *enc = ic->streams[i]->codec;
            switch(enc->codec_type) {
            case CODEC_TYPE_AUDIO:
                has_audio = 1;
                break;
            case CODEC_TYPE_VIDEO:
                has_video = 1;
                break;
            case CODEC_TYPE_SUBTITLE:
                has_subtitle = 1;
                break;
            case CODEC_TYPE_DATA:
            case CODEC_TYPE_ATTACHMENT:
            case CODEC_TYPE_UNKNOWN:
                break;
            default:
                abort();
            }
        }
    }
    *has_video_ptr = has_video;
    *has_audio_ptr = has_audio;
    *has_subtitle_ptr = has_subtitle;
}

static void new_video_stream(AVFormatContext *oc)
{
    AVStream *st;
    AVCodecContext *video_enc;
    int codec_id;

    st = av_new_stream(oc, oc->nb_streams);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        av_exit(1);
    }
    avcodec_get_context_defaults2(st->codec, CODEC_TYPE_VIDEO);
    bitstream_filters[nb_output_files][oc->nb_streams - 1]= video_bitstream_filters;
    video_bitstream_filters= NULL;

    if(thread_count>1)
        avcodec_thread_init(st->codec, thread_count);

    video_enc = st->codec;

    if(video_codec_tag)
        video_enc->codec_tag= video_codec_tag;

    if(   (video_global_header&1)
       || (video_global_header==0 && (oc->oformat->flags & AVFMT_GLOBALHEADER))){
        video_enc->flags |= CODEC_FLAG_GLOBAL_HEADER;
        avctx_opts[CODEC_TYPE_VIDEO]->flags|= CODEC_FLAG_GLOBAL_HEADER;
    }
    if(video_global_header&2){
        video_enc->flags2 |= CODEC_FLAG2_LOCAL_HEADER;
        avctx_opts[CODEC_TYPE_VIDEO]->flags2|= CODEC_FLAG2_LOCAL_HEADER;
    }

    if (video_stream_copy) {
        st->stream_copy = 1;
        video_enc->codec_type = CODEC_TYPE_VIDEO;
        video_enc->sample_aspect_ratio =
        st->sample_aspect_ratio = av_d2q(frame_aspect_ratio*frame_height/frame_width, 255);
    } else {
        const char *p;
        int i;
        AVCodec *codec;
        AVRational fps= frame_rate.num ? frame_rate : (AVRational){25,1};

        if (video_codec_name) {
            codec_id = find_codec_or_die(video_codec_name, CODEC_TYPE_VIDEO, 1);
            codec = avcodec_find_encoder_by_name(video_codec_name);
            output_codecs[nb_ocodecs] = codec;
        } else {
            codec_id = av_guess_codec(oc->oformat, NULL, oc->filename, NULL, CODEC_TYPE_VIDEO);
            codec = avcodec_find_encoder(codec_id);
        }

        video_enc->codec_id = codec_id;

        set_context_opts(video_enc, avctx_opts[CODEC_TYPE_VIDEO], AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM);

        if (codec && codec->supported_framerates && !force_fps)
            fps = codec->supported_framerates[av_find_nearest_q_idx(fps, codec->supported_framerates)];
        video_enc->time_base.den = fps.num;
        video_enc->time_base.num = fps.den;

        video_enc->width = frame_width + frame_padright + frame_padleft;
        video_enc->height = frame_height + frame_padtop + frame_padbottom;
        video_enc->sample_aspect_ratio = av_d2q(frame_aspect_ratio*video_enc->height/video_enc->width, 255);
        video_enc->pix_fmt = frame_pix_fmt;
        st->sample_aspect_ratio = video_enc->sample_aspect_ratio;

        if(codec && codec->pix_fmts){
            const enum PixelFormat *p= codec->pix_fmts;
            for(; *p!=-1; p++){
                if(*p == video_enc->pix_fmt)
                    break;
            }
            if(*p == -1)
                video_enc->pix_fmt = codec->pix_fmts[0];
        }

        if (intra_only)
            video_enc->gop_size = 0;
        if (video_qscale || same_quality) {
            video_enc->flags |= CODEC_FLAG_QSCALE;
            video_enc->global_quality=
                st->quality = FF_QP2LAMBDA * video_qscale;
        }

        if(intra_matrix)
            video_enc->intra_matrix = intra_matrix;
        if(inter_matrix)
            video_enc->inter_matrix = inter_matrix;

        video_enc->thread_count = thread_count;
        p= video_rc_override_string;
        for(i=0; p; i++){
            int start, end, q;
            int e=sscanf(p, "%d,%d,%d", &start, &end, &q);
            if(e!=3){
                fprintf(stderr, "error parsing rc_override\n");
                av_exit(1);
            }
            video_enc->rc_override=
                av_realloc(video_enc->rc_override,
                           sizeof(RcOverride)*(i+1));
            video_enc->rc_override[i].start_frame= start;
            video_enc->rc_override[i].end_frame  = end;
            if(q>0){
                video_enc->rc_override[i].qscale= q;
                video_enc->rc_override[i].quality_factor= 1.0;
            }
            else{
                video_enc->rc_override[i].qscale= 0;
                video_enc->rc_override[i].quality_factor= -q/100.0;
            }
            p= strchr(p, '/');
            if(p) p++;
        }
        video_enc->rc_override_count=i;
        if (!video_enc->rc_initial_buffer_occupancy)
            video_enc->rc_initial_buffer_occupancy = video_enc->rc_buffer_size*3/4;
        video_enc->me_threshold= me_threshold;
        video_enc->intra_dc_precision= intra_dc_precision - 8;

        if (do_psnr)
            video_enc->flags|= CODEC_FLAG_PSNR;

        /* two pass mode */
        if (do_pass) {
            if (do_pass == 1) {
                video_enc->flags |= CODEC_FLAG_PASS1;
            } else {
                video_enc->flags |= CODEC_FLAG_PASS2;
            }
        }
    }
    nb_ocodecs++;

    /* reset some key parameters */
    video_disable = 0;
    av_freep(&video_codec_name);
    video_stream_copy = 0;
}

static void new_audio_stream(AVFormatContext *oc)
{
    AVStream *st;
    AVCodecContext *audio_enc;
    int codec_id;

    st = av_new_stream(oc, oc->nb_streams);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        av_exit(1);
    }
    avcodec_get_context_defaults2(st->codec, CODEC_TYPE_AUDIO);

    bitstream_filters[nb_output_files][oc->nb_streams - 1]= audio_bitstream_filters;
    audio_bitstream_filters= NULL;

    if(thread_count>1)
        avcodec_thread_init(st->codec, thread_count);

    audio_enc = st->codec;
    audio_enc->codec_type = CODEC_TYPE_AUDIO;

    if(audio_codec_tag)
        audio_enc->codec_tag= audio_codec_tag;

    if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
        audio_enc->flags |= CODEC_FLAG_GLOBAL_HEADER;
        avctx_opts[CODEC_TYPE_AUDIO]->flags|= CODEC_FLAG_GLOBAL_HEADER;
    }
    if (audio_stream_copy) {
        st->stream_copy = 1;
        audio_enc->channels = audio_channels;
    } else {
        AVCodec *codec;

        set_context_opts(audio_enc, avctx_opts[CODEC_TYPE_AUDIO], AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_ENCODING_PARAM);

        if (audio_codec_name) {
            codec_id = find_codec_or_die(audio_codec_name, CODEC_TYPE_AUDIO, 1);
            codec = avcodec_find_encoder_by_name(audio_codec_name);
            output_codecs[nb_ocodecs] = codec;
        } else {
            codec_id = av_guess_codec(oc->oformat, NULL, oc->filename, NULL, CODEC_TYPE_AUDIO);
            codec = avcodec_find_encoder(codec_id);
        }
        audio_enc->codec_id = codec_id;

        if (audio_qscale > QSCALE_NONE) {
            audio_enc->flags |= CODEC_FLAG_QSCALE;
            audio_enc->global_quality = st->quality = FF_QP2LAMBDA * audio_qscale;
        }
        audio_enc->thread_count = thread_count;
        audio_enc->channels = audio_channels;
        audio_enc->sample_fmt = audio_sample_fmt;
        audio_enc->channel_layout = channel_layout;

        if(codec && codec->sample_fmts){
            const enum SampleFormat *p= codec->sample_fmts;
            for(; *p!=-1; p++){
                if(*p == audio_enc->sample_fmt)
                    break;
            }
            if(*p == -1)
                audio_enc->sample_fmt = codec->sample_fmts[0];
        }
    }
    nb_ocodecs++;
    audio_enc->sample_rate = audio_sample_rate;
    audio_enc->time_base= (AVRational){1, audio_sample_rate};
    if (audio_language) {
        av_strlcpy(st->language, audio_language, sizeof(st->language));
        av_free(audio_language);
        audio_language = NULL;
    }

    /* reset some key parameters */
    audio_disable = 0;
    av_freep(&audio_codec_name);
    audio_stream_copy = 0;
}

static void new_subtitle_stream(AVFormatContext *oc)
{
    AVStream *st;
    AVCodecContext *subtitle_enc;

    st = av_new_stream(oc, oc->nb_streams);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        av_exit(1);
    }
    avcodec_get_context_defaults2(st->codec, CODEC_TYPE_SUBTITLE);

    bitstream_filters[nb_output_files][oc->nb_streams - 1]= subtitle_bitstream_filters;
    subtitle_bitstream_filters= NULL;

    subtitle_enc = st->codec;
    subtitle_enc->codec_type = CODEC_TYPE_SUBTITLE;
    if (subtitle_stream_copy) {
        st->stream_copy = 1;
    } else {
        set_context_opts(avctx_opts[CODEC_TYPE_SUBTITLE], subtitle_enc, AV_OPT_FLAG_SUBTITLE_PARAM | AV_OPT_FLAG_ENCODING_PARAM);
        subtitle_enc->codec_id = find_codec_or_die(subtitle_codec_name, CODEC_TYPE_SUBTITLE, 1);
        output_codecs[nb_ocodecs] = avcodec_find_encoder_by_name(subtitle_codec_name);
    }
    nb_ocodecs++;

    if (subtitle_language) {
        av_strlcpy(st->language, subtitle_language, sizeof(st->language));
        av_free(subtitle_language);
        subtitle_language = NULL;
    }

    subtitle_disable = 0;
    av_freep(&subtitle_codec_name);
    subtitle_stream_copy = 0;
}

static void opt_new_audio_stream(void)
{
    AVFormatContext *oc;
    if (nb_output_files <= 0) {
        fprintf(stderr, "At least one output file must be specified\n");
        av_exit(1);
    }
    oc = output_files[nb_output_files - 1];
    new_audio_stream(oc);
}

static void opt_new_video_stream(void)
{
    AVFormatContext *oc;
    if (nb_output_files <= 0) {
        fprintf(stderr, "At least one output file must be specified\n");
        av_exit(1);
    }
    oc = output_files[nb_output_files - 1];
    new_video_stream(oc);
}

static void opt_new_subtitle_stream(void)
{
    AVFormatContext *oc;
    if (nb_output_files <= 0) {
        fprintf(stderr, "At least one output file must be specified\n");
        av_exit(1);
    }
    oc = output_files[nb_output_files - 1];
    new_subtitle_stream(oc);
}

static void opt_output_file(const char *filename)
{
    AVFormatContext *oc;
    int use_video, use_audio, use_subtitle;
    int input_has_video, input_has_audio, input_has_subtitle;
    AVFormatParameters params, *ap = &params;

    if (!strcmp(filename, "-"))
        filename = "pipe:";

    oc = av_alloc_format_context();

    if (!file_oformat) {
        file_oformat = guess_format(NULL, filename, NULL);
        if (!file_oformat) {
            fprintf(stderr, "Unable to find a suitable output format for '%s'\n",
                    filename);
            av_exit(1);
        }
    }

    oc->oformat = file_oformat;
    av_strlcpy(oc->filename, filename, sizeof(oc->filename));

    if (!strcmp(file_oformat->name, "ffm") &&
        av_strstart(filename, "http:", NULL)) {
        /* special case for files sent to ffserver: we get the stream
           parameters from ffserver */
        int err = read_ffserver_streams(oc, filename);
        if (err < 0) {
            print_error(filename, err);
            av_exit(1);
        }
    } else {
        use_video = file_oformat->video_codec != CODEC_ID_NONE || video_stream_copy || video_codec_name;
        use_audio = file_oformat->audio_codec != CODEC_ID_NONE || audio_stream_copy || audio_codec_name;
        use_subtitle = file_oformat->subtitle_codec != CODEC_ID_NONE || subtitle_stream_copy || subtitle_codec_name;

        /* disable if no corresponding type found and at least one
           input file */
        if (nb_input_files > 0) {
            check_audio_video_sub_inputs(&input_has_video, &input_has_audio,
                                         &input_has_subtitle);
            if (!input_has_video)
                use_video = 0;
            if (!input_has_audio)
                use_audio = 0;
            if (!input_has_subtitle)
                use_subtitle = 0;
        }

        /* manual disable */
        if (audio_disable) {
            use_audio = 0;
        }
        if (video_disable) {
            use_video = 0;
        }
        if (subtitle_disable) {
            use_subtitle = 0;
        }

        if (use_video) {
            new_video_stream(oc);
        }

        if (use_audio) {
            new_audio_stream(oc);
        }

        if (use_subtitle) {
            new_subtitle_stream(oc);
        }

        oc->timestamp = rec_timestamp;

        if (str_title)
            av_strlcpy(oc->title, str_title, sizeof(oc->title));
        if (str_author)
            av_strlcpy(oc->author, str_author, sizeof(oc->author));
        if (str_copyright)
            av_strlcpy(oc->copyright, str_copyright, sizeof(oc->copyright));
        if (str_comment)
            av_strlcpy(oc->comment, str_comment, sizeof(oc->comment));
        if (str_album)
            av_strlcpy(oc->album, str_album, sizeof(oc->album));
        if (str_genre)
            av_strlcpy(oc->genre, str_genre, sizeof(oc->genre));
    }

    output_files[nb_output_files++] = oc;

    /* check filename in case of an image number is expected */
    if (oc->oformat->flags & AVFMT_NEEDNUMBER) {
        if (!av_filename_number_test(oc->filename)) {
            print_error(oc->filename, AVERROR_NUMEXPECTED);
            av_exit(1);
        }
    }

    if (!(oc->oformat->flags & AVFMT_NOFILE)) {
        /* test if it already exists to avoid loosing precious files */
        if (!file_overwrite &&
            (strchr(filename, ':') == NULL ||
             filename[1] == ':' ||
             av_strstart(filename, "file:", NULL))) {
            if (url_exist(filename)) {
                int c;

                if (!using_stdin) {
                    fprintf(stderr,"File '%s' already exists. Overwrite ? [y/N] ", filename);
                    fflush(stderr);
                    c = getchar();
                    if (toupper(c) != 'Y') {
                        fprintf(stderr, "Not overwriting - exiting\n");
                        av_exit(1);
                    }
                }
                else {
                    fprintf(stderr,"File '%s' already exists. Exiting.\n", filename);
                    av_exit(1);
                }
            }
        }

        /* open the file */
        if (url_fopen(&oc->pb, filename, URL_WRONLY) < 0) {
            fprintf(stderr, "Could not open '%s'\n", filename);
            av_exit(1);
        }
    }

    memset(ap, 0, sizeof(*ap));
    if (av_set_parameters(oc, ap) < 0) {
        fprintf(stderr, "%s: Invalid encoding parameters\n",
                oc->filename);
        av_exit(1);
    }

    oc->preload= (int)(mux_preload*AV_TIME_BASE);
    oc->max_delay= (int)(mux_max_delay*AV_TIME_BASE);
    oc->loop_output = loop_output;

    set_context_opts(oc, avformat_opts, AV_OPT_FLAG_ENCODING_PARAM);

    /* reset some options */
    file_oformat = NULL;
    file_iformat = NULL;
}

/* same option as mencoder */
static void opt_pass(const char *pass_str)
{
    int pass;
    pass = atoi(pass_str);
    if (pass != 1 && pass != 2) {
        fprintf(stderr, "pass number can be only 1 or 2\n");
        av_exit(1);
    }
    do_pass = pass;
}

static int64_t getutime(void)
{
#ifdef HAVE_GETRUSAGE
    struct rusage rusage;

    getrusage(RUSAGE_SELF, &rusage);
    return (rusage.ru_utime.tv_sec * 1000000LL) + rusage.ru_utime.tv_usec;
#elif defined(HAVE_GETPROCESSTIMES)
    HANDLE proc;
    FILETIME c, e, k, u;
    proc = GetCurrentProcess();
    GetProcessTimes(proc, &c, &e, &k, &u);
    return ((int64_t) u.dwHighDateTime << 32 | u.dwLowDateTime) / 10;
#else
    return av_gettime();
#endif
}

static void parse_matrix_coeffs(uint16_t *dest, const char *str)
{
    int i;
    const char *p = str;
    for(i = 0;; i++) {
        dest[i] = atoi(p);
        if(i == 63)
            break;
        p = strchr(p, ',');
        if(!p) {
            fprintf(stderr, "Syntax error in matrix \"%s\" at coeff %d\n", str, i);
            av_exit(1);
        }
        p++;
    }
}

static void opt_inter_matrix(const char *arg)
{
    inter_matrix = av_mallocz(sizeof(uint16_t) * 64);
    parse_matrix_coeffs(inter_matrix, arg);
}

static void opt_intra_matrix(const char *arg)
{
    intra_matrix = av_mallocz(sizeof(uint16_t) * 64);
    parse_matrix_coeffs(intra_matrix, arg);
}

/**
 * Trivial log callback.
 * Only suitable for show_help and similar since it lacks prefix handling.
 */
static void log_callback_help(void* ptr, int level, const char* fmt, va_list vl)
{
    vfprintf(stdout, fmt, vl);
}

static void show_help(void)
{
    av_log_set_callback(log_callback_help);
    printf("usage: ffmpeg [[infile options] -i infile]... {[outfile options] outfile}...\n"
           "Hyper fast Audio and Video encoder\n");
    printf("\n");
    show_help_options(options, "Main options:\n",
                      OPT_EXPERT | OPT_AUDIO | OPT_VIDEO | OPT_SUBTITLE | OPT_GRAB, 0);
    show_help_options(options, "\nAdvanced options:\n",
                      OPT_EXPERT | OPT_AUDIO | OPT_VIDEO | OPT_SUBTITLE | OPT_GRAB,
                      OPT_EXPERT);
    show_help_options(options, "\nVideo options:\n",
                      OPT_EXPERT | OPT_AUDIO | OPT_VIDEO | OPT_GRAB,
                      OPT_VIDEO);
    show_help_options(options, "\nAdvanced Video options:\n",
                      OPT_EXPERT | OPT_AUDIO | OPT_VIDEO | OPT_GRAB,
                      OPT_VIDEO | OPT_EXPERT);
    show_help_options(options, "\nAudio options:\n",
                      OPT_EXPERT | OPT_AUDIO | OPT_VIDEO | OPT_GRAB,
                      OPT_AUDIO);
    show_help_options(options, "\nAdvanced Audio options:\n",
                      OPT_EXPERT | OPT_AUDIO | OPT_VIDEO | OPT_GRAB,
                      OPT_AUDIO | OPT_EXPERT);
    show_help_options(options, "\nSubtitle options:\n",
                      OPT_SUBTITLE | OPT_GRAB,
                      OPT_SUBTITLE);
    show_help_options(options, "\nAudio/Video grab options:\n",
                      OPT_GRAB,
                      OPT_GRAB);
    printf("\n");
    av_opt_show(avctx_opts[0], NULL);
    printf("\n");
    av_opt_show(avformat_opts, NULL);
    printf("\n");
    av_opt_show(sws_opts, NULL);
}

static void opt_target(const char *arg)
{
    int norm = -1;
    static const char *const frame_rates[] = {"25", "30000/1001", "24000/1001"};

    if(!strncmp(arg, "pal-", 4)) {
        norm = 0;
        arg += 4;
    } else if(!strncmp(arg, "ntsc-", 5)) {
        norm = 1;
        arg += 5;
    } else if(!strncmp(arg, "film-", 5)) {
        norm = 2;
        arg += 5;
    } else {
        int fr;
        /* Calculate FR via float to avoid int overflow */
        fr = (int)(frame_rate.num * 1000.0 / frame_rate.den);
        if(fr == 25000) {
            norm = 0;
        } else if((fr == 29970) || (fr == 23976)) {
            norm = 1;
        } else {
            /* Try to determine PAL/NTSC by peeking in the input files */
            if(nb_input_files) {
                int i, j;
                for(j = 0; j < nb_input_files; j++) {
                    for(i = 0; i < input_files[j]->nb_streams; i++) {
                        AVCodecContext *c = input_files[j]->streams[i]->codec;
                        if(c->codec_type != CODEC_TYPE_VIDEO)
                            continue;
                        fr = c->time_base.den * 1000 / c->time_base.num;
                        if(fr == 25000) {
                            norm = 0;
                            break;
                        } else if((fr == 29970) || (fr == 23976)) {
                            norm = 1;
                            break;
                        }
                    }
                    if(norm >= 0)
                        break;
                }
            }
        }
        if(verbose && norm >= 0)
            fprintf(stderr, "Assuming %s for target.\n", norm ? "NTSC" : "PAL");
    }

    if(norm < 0) {
        fprintf(stderr, "Could not determine norm (PAL/NTSC/NTSC-Film) for target.\n");
        fprintf(stderr, "Please prefix target with \"pal-\", \"ntsc-\" or \"film-\",\n");
        fprintf(stderr, "or set a framerate with \"-r xxx\".\n");
        av_exit(1);
    }

    if(!strcmp(arg, "vcd")) {

        opt_video_codec("mpeg1video");
        opt_audio_codec("mp2");
        opt_format("vcd");

        opt_frame_size(norm ? "352x240" : "352x288");
        opt_frame_rate(NULL, frame_rates[norm]);
        opt_default("gop", norm ? "18" : "15");

        opt_default("b", "1150000");
        opt_default("maxrate", "1150000");
        opt_default("minrate", "1150000");
        opt_default("bufsize", "327680"); // 40*1024*8;

        opt_default("ab", "224000");
        audio_sample_rate = 44100;
        audio_channels = 2;

        opt_default("packetsize", "2324");
        opt_default("muxrate", "1411200"); // 2352 * 75 * 8;

        /* We have to offset the PTS, so that it is consistent with the SCR.
           SCR starts at 36000, but the first two packs contain only padding
           and the first pack from the other stream, respectively, may also have
           been written before.
           So the real data starts at SCR 36000+3*1200. */
        mux_preload= (36000+3*1200) / 90000.0; //0.44
    } else if(!strcmp(arg, "svcd")) {

        opt_video_codec("mpeg2video");
        opt_audio_codec("mp2");
        opt_format("svcd");

        opt_frame_size(norm ? "480x480" : "480x576");
        opt_frame_rate(NULL, frame_rates[norm]);
        opt_default("gop", norm ? "18" : "15");

        opt_default("b", "2040000");
        opt_default("maxrate", "2516000");
        opt_default("minrate", "0"); //1145000;
        opt_default("bufsize", "1835008"); //224*1024*8;
        opt_default("flags", "+scan_offset");


        opt_default("ab", "224000");
        audio_sample_rate = 44100;

        opt_default("packetsize", "2324");

    } else if(!strcmp(arg, "dvd")) {

        opt_video_codec("mpeg2video");
        opt_audio_codec("ac3");
        opt_format("dvd");

        opt_frame_size(norm ? "720x480" : "720x576");
        opt_frame_rate(NULL, frame_rates[norm]);
        opt_default("gop", norm ? "18" : "15");

        opt_default("b", "6000000");
        opt_default("maxrate", "9000000");
        opt_default("minrate", "0"); //1500000;
        opt_default("bufsize", "1835008"); //224*1024*8;

        opt_default("packetsize", "2048");  // from www.mpucoder.com: DVD sectors contain 2048 bytes of data, this is also the size of one pack.
        opt_default("muxrate", "10080000"); // from mplex project: data_rate = 1260000. mux_rate = data_rate * 8

        opt_default("ab", "448000");
        audio_sample_rate = 48000;

    } else if(!strncmp(arg, "dv", 2)) {

        opt_format("dv");

        opt_frame_size(norm ? "720x480" : "720x576");
        opt_frame_pix_fmt(!strncmp(arg, "dv50", 4) ? "yuv422p" :
                                             (norm ? "yuv411p" : "yuv420p"));
        opt_frame_rate(NULL, frame_rates[norm]);

        audio_sample_rate = 48000;
        audio_channels = 2;

    } else {
        fprintf(stderr, "Unknown target: %s\n", arg);
        av_exit(1);
    }
}

static void opt_vstats_file (const char *arg)
{
    av_free (vstats_filename);
    vstats_filename=av_strdup (arg);
}

static void opt_vstats (void)
{
    char filename[40];
    time_t today2 = time(NULL);
    struct tm *today = localtime(&today2);

    snprintf(filename, sizeof(filename), "vstats_%02d%02d%02d.log", today->tm_hour, today->tm_min,
             today->tm_sec);
    opt_vstats_file(filename);
}

static int opt_bsf(const char *opt, const char *arg)
{
    AVBitStreamFilterContext *bsfc= av_bitstream_filter_init(arg); //FIXME split name and args for filter at '='
    AVBitStreamFilterContext **bsfp;

    if(!bsfc){
        fprintf(stderr, "Unknown bitstream filter %s\n", arg);
        av_exit(1);
    }

    bsfp= *opt == 'v' ? &video_bitstream_filters :
          *opt == 'a' ? &audio_bitstream_filters :
                        &subtitle_bitstream_filters;
    while(*bsfp)
        bsfp= &(*bsfp)->next;

    *bsfp= bsfc;

    return 0;
}

static int opt_preset(const char *opt, const char *arg)
{
    FILE *f=NULL;
    char filename[1000], tmp[1000], tmp2[1000], line[1000];
    int i;
    const char *base[3]= { getenv("HOME"),
                           "/usr/local/share",
                           "/usr/share",
                         };

    for(i=!base[0]; i<3 && !f; i++){
        snprintf(filename, sizeof(filename), "%s/%sffmpeg/%s.ffpreset", base[i], i ? "" : ".", arg);
        f= fopen(filename, "r");
        if(!f){
            char *codec_name= *opt == 'v' ? video_codec_name :
                              *opt == 'a' ? audio_codec_name :
                                            subtitle_codec_name;
            snprintf(filename, sizeof(filename), "%s/%sffmpeg/%s-%s.ffpreset", base[i],  i ? "" : ".", codec_name, arg);
            f= fopen(filename, "r");
        }
    }
    if(!f && ((arg[0]=='.' && arg[1]=='/') || arg[0]=='/' ||
              is_dos_path(arg))){
        snprintf(filename, sizeof(filename), arg);
        f= fopen(filename, "r");
    }

    if(!f){
        fprintf(stderr, "File for preset '%s' not found\n", arg);
        av_exit(1);
    }

    while(!feof(f)){
        int e= fscanf(f, "%999[^\n]\n", line) - 1;
        if(line[0] == '#' && !e)
            continue;
        e|= sscanf(line, "%999[^=]=%999[^\n]\n", tmp, tmp2) - 2;
        if(e){
            fprintf(stderr, "%s: Invalid syntax: '%s'\n", filename, line);
            av_exit(1);
        }
        if(!strcmp(tmp, "acodec")){
            opt_audio_codec(tmp2);
        }else if(!strcmp(tmp, "vcodec")){
            opt_video_codec(tmp2);
        }else if(!strcmp(tmp, "scodec")){
            opt_subtitle_codec(tmp2);
        }else if(opt_default(tmp, tmp2) < 0){
            fprintf(stderr, "%s: Invalid option or argument: '%s', parsed as '%s' = '%s'\n", filename, line, tmp, tmp2);
            av_exit(1);
        }
    }

    fclose(f);

    return 0;
}

static const OptionDef options[] = {
    /* main options */
    { "L", OPT_EXIT, {(void*)show_license}, "show license" },
    { "h", OPT_EXIT, {(void*)show_help}, "show help" },
    { "version", OPT_EXIT, {(void*)show_version}, "show version" },
    { "formats", OPT_EXIT, {(void*)show_formats}, "show available formats, codecs, protocols, ..." },
    { "f", HAS_ARG, {(void*)opt_format}, "force format", "fmt" },
    { "i", HAS_ARG, {(void*)opt_input_file}, "input file name", "filename" },
    { "y", OPT_BOOL, {(void*)&file_overwrite}, "overwrite output files" },
    { "map", HAS_ARG | OPT_EXPERT, {(void*)opt_map}, "set input stream mapping", "file:stream[:syncfile:syncstream]" },
    { "map_meta_data", HAS_ARG | OPT_EXPERT, {(void*)opt_map_meta_data}, "set meta data information of outfile from infile", "outfile:infile" },
    { "t", OPT_FUNC2 | HAS_ARG, {(void*)opt_recording_time}, "record or transcode \"duration\" seconds of audio/video", "duration" },
    { "fs", HAS_ARG | OPT_INT64, {(void*)&limit_filesize}, "set the limit file size in bytes", "limit_size" }, //
    { "ss", OPT_FUNC2 | HAS_ARG, {(void*)opt_start_time}, "set the start time offset", "time_off" },
    { "itsoffset", OPT_FUNC2 | HAS_ARG, {(void*)opt_input_ts_offset}, "set the input ts offset", "time_off" },
    { "itsscale", HAS_ARG, {(void*)opt_input_ts_scale}, "set the input ts scale", "stream:scale" },
    { "title", HAS_ARG | OPT_STRING, {(void*)&str_title}, "set the title", "string" },
    { "timestamp", OPT_FUNC2 | HAS_ARG, {(void*)&opt_rec_timestamp}, "set the timestamp", "time" },
    { "author", HAS_ARG | OPT_STRING, {(void*)&str_author}, "set the author", "string" },
    { "copyright", HAS_ARG | OPT_STRING, {(void*)&str_copyright}, "set the copyright", "string" },
    { "comment", HAS_ARG | OPT_STRING, {(void*)&str_comment}, "set the comment", "string" },
    { "genre", HAS_ARG | OPT_STRING, {(void*)&str_genre}, "set the genre", "string" },
    { "album", HAS_ARG | OPT_STRING, {(void*)&str_album}, "set the album", "string" },
    { "dframes", OPT_INT | HAS_ARG, {(void*)&max_frames[CODEC_TYPE_DATA]}, "set the number of data frames to record", "number" },
    { "benchmark", OPT_BOOL | OPT_EXPERT, {(void*)&do_benchmark},
      "add timings for benchmarking" },
    { "dump", OPT_BOOL | OPT_EXPERT, {(void*)&do_pkt_dump},
      "dump each input packet" },
    { "hex", OPT_BOOL | OPT_EXPERT, {(void*)&do_hex_dump},
      "when dumping packets, also dump the payload" },
    { "re", OPT_BOOL | OPT_EXPERT, {(void*)&rate_emu}, "read input at native frame rate", "" },
    { "loop_input", OPT_BOOL | OPT_EXPERT, {(void*)&loop_input}, "loop (current only works with images)" },
    { "loop_output", HAS_ARG | OPT_INT | OPT_EXPERT, {(void*)&loop_output}, "number of times to loop output in formats that support looping (0 loops forever)", "" },
    { "v", HAS_ARG | OPT_FUNC2, {(void*)opt_verbose}, "set the logging verbosity level", "number" },
    { "target", HAS_ARG, {(void*)opt_target}, "specify target file type (\"vcd\", \"svcd\", \"dvd\", \"dv\", \"dv50\", \"pal-vcd\", \"ntsc-svcd\", ...)", "type" },
    { "threads", OPT_FUNC2 | HAS_ARG | OPT_EXPERT, {(void*)opt_thread_count}, "thread count", "count" },
    { "vsync", HAS_ARG | OPT_INT | OPT_EXPERT, {(void*)&video_sync_method}, "video sync method", "" },
    { "async", HAS_ARG | OPT_INT | OPT_EXPERT, {(void*)&audio_sync_method}, "audio sync method", "" },
    { "adrift_threshold", HAS_ARG | OPT_FLOAT | OPT_EXPERT, {(void*)&audio_drift_threshold}, "audio drift threshold", "threshold" },
    { "vglobal", HAS_ARG | OPT_INT | OPT_EXPERT, {(void*)&video_global_header}, "video global header storage type", "" },
    { "copyts", OPT_BOOL | OPT_EXPERT, {(void*)&copy_ts}, "copy timestamps" },
    { "shortest", OPT_BOOL | OPT_EXPERT, {(void*)&opt_shortest}, "finish encoding within shortest input" }, //
    { "dts_delta_threshold", HAS_ARG | OPT_FLOAT | OPT_EXPERT, {(void*)&dts_delta_threshold}, "timestamp discontinuity delta threshold", "threshold" },
    { "programid", HAS_ARG | OPT_INT | OPT_EXPERT, {(void*)&opt_programid}, "desired program number", "" },
    { "xerror", OPT_BOOL, {(void*)&exit_on_error}, "exit on error", "error" },

    /* video options */
    { "b", OPT_FUNC2 | HAS_ARG | OPT_VIDEO, {(void*)opt_bitrate}, "set bitrate (in bits/s)", "bitrate" },
    { "vb", OPT_FUNC2 | HAS_ARG | OPT_VIDEO, {(void*)opt_bitrate}, "set bitrate (in bits/s)", "bitrate" },
    { "vframes", OPT_INT | HAS_ARG | OPT_VIDEO, {(void*)&max_frames[CODEC_TYPE_VIDEO]}, "set the number of video frames to record", "number" },
    { "r", OPT_FUNC2 | HAS_ARG | OPT_VIDEO, {(void*)opt_frame_rate}, "set frame rate (Hz value, fraction or abbreviation)", "rate" },
    { "s", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_size}, "set frame size (WxH or abbreviation)", "size" },
    { "aspect", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_aspect_ratio}, "set aspect ratio (4:3, 16:9 or 1.3333, 1.7777)", "aspect" },
    { "pix_fmt", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_frame_pix_fmt}, "set pixel format, 'list' as argument shows all the pixel formats supported", "format" },
    { "croptop", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_crop_top}, "set top crop band size (in pixels)", "size" },
    { "cropbottom", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_crop_bottom}, "set bottom crop band size (in pixels)", "size" },
    { "cropleft", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_crop_left}, "set left crop band size (in pixels)", "size" },
    { "cropright", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_crop_right}, "set right crop band size (in pixels)", "size" },
    { "padtop", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_pad_top}, "set top pad band size (in pixels)", "size" },
    { "padbottom", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_pad_bottom}, "set bottom pad band size (in pixels)", "size" },
    { "padleft", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_pad_left}, "set left pad band size (in pixels)", "size" },
    { "padright", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_pad_right}, "set right pad band size (in pixels)", "size" },
    { "padcolor", HAS_ARG | OPT_VIDEO, {(void*)opt_pad_color}, "set color of pad bands (Hex 000000 thru FFFFFF)", "color" },
    { "intra", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, {(void*)&intra_only}, "use only intra frames"},
    { "vn", OPT_BOOL | OPT_VIDEO, {(void*)&video_disable}, "disable video" },
    { "vdt", OPT_INT | HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)&video_discard}, "discard threshold", "n" },
    { "qscale", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_qscale}, "use fixed video quantizer scale (VBR)", "q" },
    { "rc_override", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_video_rc_override_string}, "rate control override for specific intervals", "override" },
    { "vcodec", HAS_ARG | OPT_VIDEO, {(void*)opt_video_codec}, "force video codec ('copy' to copy stream)", "codec" },
    { "me_threshold", HAS_ARG | OPT_FUNC2 | OPT_EXPERT | OPT_VIDEO, {(void*)opt_me_threshold}, "motion estimaton threshold",  "threshold" },
    { "sameq", OPT_BOOL | OPT_VIDEO, {(void*)&same_quality},
      "use same video quality as source (implies VBR)" },
    { "pass", HAS_ARG | OPT_VIDEO, {(void*)&opt_pass}, "select the pass number (1 or 2)", "n" },
    { "passlogfile", HAS_ARG | OPT_STRING | OPT_VIDEO, {(void*)&pass_logfilename}, "select two pass log file name", "file" },
    { "deinterlace", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, {(void*)&do_deinterlace},
      "deinterlace pictures" },
    { "psnr", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, {(void*)&do_psnr}, "calculate PSNR of compressed frames" },
    { "vstats", OPT_EXPERT | OPT_VIDEO, {(void*)&opt_vstats}, "dump video coding statistics to file" },
    { "vstats_file", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_vstats_file}, "dump video coding statistics to file", "file" },
#ifdef CONFIG_VHOOK
    { "vhook", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)add_frame_hooker}, "insert video processing module", "module" },
#endif
    { "intra_matrix", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_intra_matrix}, "specify intra matrix coeffs", "matrix" },
    { "inter_matrix", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_inter_matrix}, "specify inter matrix coeffs", "matrix" },
    { "top", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_top_field_first}, "top=1/bottom=0/auto=-1 field first", "" },
    { "dc", OPT_INT | HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)&intra_dc_precision}, "intra_dc_precision", "precision" },
    { "vtag", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_video_tag}, "force video tag/fourcc", "fourcc/tag" },
    { "newvideo", OPT_VIDEO, {(void*)opt_new_video_stream}, "add a new video stream to the current output stream" },
    { "qphist", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, { (void *)&qp_hist }, "show QP histogram" },
    { "force_fps", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, {(void*)&force_fps}, "force the selected framerate, disable the best supported framerate selection" },

    /* audio options */
    { "ab", OPT_FUNC2 | HAS_ARG | OPT_AUDIO, {(void*)opt_bitrate}, "set bitrate (in bits/s)", "bitrate" },
    { "aframes", OPT_INT | HAS_ARG | OPT_AUDIO, {(void*)&max_frames[CODEC_TYPE_AUDIO]}, "set the number of audio frames to record", "number" },
    { "aq", OPT_FLOAT | HAS_ARG | OPT_AUDIO, {(void*)&audio_qscale}, "set audio quality (codec-specific)", "quality", },
    { "ar", HAS_ARG | OPT_FUNC2 | OPT_AUDIO, {(void*)opt_audio_rate}, "set audio sampling rate (in Hz)", "rate" },
    { "ac", HAS_ARG | OPT_FUNC2 | OPT_AUDIO, {(void*)opt_audio_channels}, "set number of audio channels", "channels" },
    { "an", OPT_BOOL | OPT_AUDIO, {(void*)&audio_disable}, "disable audio" },
    { "acodec", HAS_ARG | OPT_AUDIO, {(void*)opt_audio_codec}, "force audio codec ('copy' to copy stream)", "codec" },
    { "atag", HAS_ARG | OPT_EXPERT | OPT_AUDIO, {(void*)opt_audio_tag}, "force audio tag/fourcc", "fourcc/tag" },
    { "vol", OPT_INT | HAS_ARG | OPT_AUDIO, {(void*)&audio_volume}, "change audio volume (256=normal)" , "volume" }, //
    { "newaudio", OPT_AUDIO, {(void*)opt_new_audio_stream}, "add a new audio stream to the current output stream" },
    { "alang", HAS_ARG | OPT_STRING | OPT_AUDIO, {(void *)&audio_language}, "set the ISO 639 language code (3 letters) of the current audio stream" , "code" },
    { "sample_fmt", HAS_ARG | OPT_EXPERT | OPT_AUDIO, {(void*)opt_audio_sample_fmt}, "set sample format, 'list' as argument shows all the sample formats supported", "format" },

    /* subtitle options */
    { "sn", OPT_BOOL | OPT_SUBTITLE, {(void*)&subtitle_disable}, "disable subtitle" },
    { "scodec", HAS_ARG | OPT_SUBTITLE, {(void*)opt_subtitle_codec}, "force subtitle codec ('copy' to copy stream)", "codec" },
    { "newsubtitle", OPT_SUBTITLE, {(void*)opt_new_subtitle_stream}, "add a new subtitle stream to the current output stream" },
    { "slang", HAS_ARG | OPT_STRING | OPT_SUBTITLE, {(void *)&subtitle_language}, "set the ISO 639 language code (3 letters) of the current subtitle stream" , "code" },

    /* grab options */
    { "vc", HAS_ARG | OPT_EXPERT | OPT_VIDEO | OPT_GRAB, {(void*)opt_video_channel}, "set video grab channel (DV1394 only)", "channel" },
    { "tvstd", HAS_ARG | OPT_EXPERT | OPT_VIDEO | OPT_GRAB, {(void*)opt_video_standard}, "set television standard (NTSC, PAL (SECAM))", "standard" },
    { "isync", OPT_BOOL | OPT_EXPERT | OPT_GRAB, {(void*)&input_sync}, "sync read on input", "" },

    /* muxer options */
    { "muxdelay", OPT_FLOAT | HAS_ARG | OPT_EXPERT, {(void*)&mux_max_delay}, "set the maximum demux-decode delay", "seconds" },
    { "muxpreload", OPT_FLOAT | HAS_ARG | OPT_EXPERT, {(void*)&mux_preload}, "set the initial demux-decode delay", "seconds" },

    { "absf", OPT_FUNC2 | HAS_ARG | OPT_AUDIO | OPT_EXPERT, {(void*)opt_bsf}, "", "bitstream_filter" },
    { "vbsf", OPT_FUNC2 | HAS_ARG | OPT_VIDEO | OPT_EXPERT, {(void*)opt_bsf}, "", "bitstream_filter" },
    { "sbsf", OPT_FUNC2 | HAS_ARG | OPT_SUBTITLE | OPT_EXPERT, {(void*)opt_bsf}, "", "bitstream_filter" },

    { "apre", OPT_FUNC2 | HAS_ARG | OPT_AUDIO | OPT_EXPERT, {(void*)opt_preset}, "set the audio options to the indicated preset", "preset" },
    { "vpre", OPT_FUNC2 | HAS_ARG | OPT_VIDEO | OPT_EXPERT, {(void*)opt_preset}, "set the video options to the indicated preset", "preset" },
    { "spre", OPT_FUNC2 | HAS_ARG | OPT_SUBTITLE | OPT_EXPERT, {(void*)opt_preset}, "set the subtitle options to the indicated preset", "preset" },

    { "default", OPT_FUNC2 | HAS_ARG | OPT_AUDIO | OPT_VIDEO | OPT_EXPERT, {(void*)opt_default}, "generic catch all option", "" },
    { NULL, },
};

int main(int argc, char **argv)
{
    int i;
    int64_t ti;

    avcodec_register_all();
    avdevice_register_all();
    av_register_all();

    if(isatty(STDIN_FILENO))
        url_set_interrupt_cb(decode_interrupt_cb);

    for(i=0; i<CODEC_TYPE_NB; i++){
        avctx_opts[i]= avcodec_alloc_context2(i);
    }
    avformat_opts = av_alloc_format_context();
    sws_opts = sws_getContext(16,16,0, 16,16,0, sws_flags, NULL,NULL,NULL);

    show_banner();

    /* parse options */
    parse_options(argc, argv, options, opt_output_file);

    /* file converter / grab */
    if (nb_output_files <= 0) {
        fprintf(stderr, "At least one output file must be specified\n");
        av_exit(1);
    }

    if (nb_input_files == 0) {
        fprintf(stderr, "At least one input file must be specified\n");
        av_exit(1);
    }

    ti = getutime();
    av_encode(output_files, nb_output_files, input_files, nb_input_files,
              stream_maps, nb_stream_maps);
    ti = getutime() - ti;
    if (do_benchmark) {
        printf("bench: utime=%0.3fs\n", ti / 1000000.0);
    }

    return av_exit(0);
}
