// example.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <opencv2/opencv.hpp>
#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswscale/swscale.h>
}

void printErrorText(int err)
{
    char szBuffer[1024];
    av_make_error_string(szBuffer, sizeof(szBuffer), err);
    std::cout << szBuffer << std::endl; 
}

int main()
{
    const int Width = 512, Height = 512, RateFactor = 45, NumFrames = 6760;
    const std::string name = "googliebah.mp4"; 

    AVFormatContext* pFormat; 
    int ret = avformat_alloc_output_context2(&pFormat, NULL, "mp4", name.c_str());
    if (ret == 0)
    {
        auto codec = avcodec_find_encoder(AV_CODEC_ID_H264); 
        if (codec)
        {
            auto newStream = avformat_new_stream(pFormat, codec); 
            if (newStream)
            {
                auto codecContext = avcodec_alloc_context3(codec); 
                if (codecContext)
                {
                    newStream->codecpar->codec_id = AV_CODEC_ID_H264;
                    newStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
                    newStream->codecpar->width = Width;
                    newStream->codecpar->height = Height;
                    newStream->codecpar->format = AV_PIX_FMT_YUV420P;
                    newStream->time_base = { 1, 75 };
                    avcodec_parameters_to_context(codecContext, newStream->codecpar);

                    codecContext->time_base = { 1, 75 };
                    codecContext->gop_size = 30;

                    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
                    double fontScale = 2;
                    int thickness = 3;
                    cv::Point textOrg(50, 50);

                    if ((ret = av_opt_set_int(codecContext, "crf", RateFactor, AV_OPT_SEARCH_CHILDREN)) == 0)
                    {
                        if ((ret = avcodec_parameters_from_context(newStream->codecpar, codecContext)) == 0)
                        {
                            if ((ret = avcodec_open2(codecContext, codec, NULL)) == 0) {
                                if ((ret = avio_open(&pFormat->pb, name.c_str(), AVIO_FLAG_WRITE)) == 0) {
                                    if ((ret = avformat_write_header(pFormat, NULL)) == 0) 
                                    {
                                        auto frame = av_frame_alloc();
                                        frame->format = AV_PIX_FMT_YUV420P;
                                        frame->width = Width; 
                                        frame->height = Height; 

                                        if ((ret = av_frame_get_buffer(frame, 32)) == 0) 
                                        {
                                            auto swsContext = sws_getContext(codecContext->width,
                                                codecContext->height, AV_PIX_FMT_RGB24, codecContext->width,
                                                codecContext->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);

                                            int inLinesize[1] = { 3 * codecContext->width };

                                            AV_TIME_BASE;
                                            AVPacket pkt;
                                            av_init_packet(&pkt);
                                            pkt.data = NULL;
                                            pkt.size = 0;
                                            pkt.flags |= AV_PKT_FLAG_KEY;

                                            for (int f = 0; f < NumFrames; f++)
                                            {
                                                auto matrix = cv::Mat(Height, Width, CV_8UC3, cv::Scalar(0, 0, 0));

                                                std::stringstream ss;
                                                ss << f; 

                                                cv::putText(matrix, ss.str().c_str(), textOrg, fontFace, fontScale, cv::Scalar(255, 255, 255), thickness, 8);

                                                auto matrixData = matrix.ptr(); 

                                                // From RGB to YUV
                                                sws_scale(swsContext, (const uint8_t* const*)&matrixData, inLinesize, 0, codecContext->height,
                                                    frame->data, frame->linesize);

                                                frame->pts = av_rescale_q(f, AVRational{ 10, 75 }, newStream->time_base);

                                                if ((ret = avcodec_send_frame(codecContext, frame)) == 0) {

                                                    ret = avcodec_receive_packet(codecContext, &pkt); 

                                                    if (ret == AVERROR(EAGAIN))
                                                    {
                                                        continue; 
                                                    }
                                                    else
                                                    {
                                                        av_interleaved_write_frame(pFormat, &pkt);
                                                    }
                                                    av_packet_unref(&pkt);
                                                }
                                            }

                                            if ((ret = avcodec_send_frame(codecContext, NULL)) == 0)
                                            {
                                                for (;;)
                                                {
                                                    if ((ret = avcodec_receive_packet(codecContext, &pkt)) == AVERROR_EOF)
                                                    {
                                                        break;
                                                    }
                                                    else
                                                    {
                                                        ret = av_interleaved_write_frame(pFormat, &pkt);
                                                        av_packet_unref(&pkt);
                                                    }
                                                }

                                                av_write_trailer(pFormat);
                                                avio_close(pFormat->pb);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        printErrorText(ret);
                                    }
                                }
                                else
                                {
                                    printErrorText(ret);
                                }
                            }
                            else
                            {
                                printErrorText(ret);
                            }
                        }
                        else
                        {
                            printErrorText(ret);
                        }
                    }
                    else
                    {
                        printErrorText(ret); 
                    }

                    avcodec_free_context(&codecContext); 
                }
            }
        }

        avformat_free_context(pFormat); 
    }
    else
    {
        printErrorText(ret); 
    }
}
