#include "AVCombination.h"
#include <algorithm>
#include <stdio.h>
#include <fstream>
#include <iostream>
#if _HAS_CXX17
#include <filesystem>

#elif _WIN64 || _WIN32  
#include <windows.h>
#elif __unix__ || __linux__ 
#include <direct.h>
#include <sys/types.h>
#include <dirent.h>
#endif

#pragma region public method
AVCombination::AVCombination() {
    // 对设备进行注册
    avdevice_register_all();
    // 初始化网络库以及网络加密协议相关的库（比如openssl）
    avformat_network_init();
}

AVCombination::~AVCombination() {
    avformat_network_deinit();
    av_free(pVideoOutFrame);
    sws_freeContext(pVideoSwsCtx);
    avcodec_free_context(&pOutEnecodecCtx);
}
std::string AVCombination::getSuffixName(std::string fullName)
{
    int index = fullName.rfind(".");
    if (index == std::string::npos || index >= (fullName.length() - 1)) {
        return "";
    }
    
    return fullName.substr(index+1, fullName.length());
}
bool AVCombination::isAudioFileName(std::string fileName)
{
	auto suffix = getSuffixName(fileName);
	if (suffix.empty()) {
		return false;
	}
	std::transform(suffix.begin(), suffix.end(), suffix.begin(), std::tolower);
	return suffix == "mp3";
}
bool AVCombination::isVideoFileName(std::string fileName)
{
	auto suffix = getSuffixName(fileName);
	if (suffix.empty()) {
		return false;
	}
	std::transform(suffix.begin(), suffix.end(), suffix.begin(), std::tolower);
    return suffix == "mp4" || suffix == "avi" || suffix == "mov" || suffix == "mpg" || suffix == "mpeg"
           || suffix == "rm" || suffix == "rmvb" || suffix == "wmv" || suffix == "asf" || suffix == "dat";
}
bool AVCombination::mergeVedio(std::string filePath1, std::string filePath2) {

    return false;
}
void AVCombination::decodeVideo(std::string filePath) {
    AVPacket* pPacket = av_packet_alloc();
    av_init_packet(pPacket);
    AVFrame* pFrame = av_frame_alloc();

    std::vector<AVPacket*> packetList;  
    // 前面视频的pts累计
    int64_t previous_pts = 0;
    // 当前视频的最后pts
    int64_t last_pts = 0;
    int videoStreamIndex = -1;
    int dealwithResult = -1;
    avcodec_free_context(&pDecodeCtx);
    dealwithResult = avformat_open_input(&pInputFormatCtx, filePath.c_str(), NULL, NULL);
    if (dealwithResult < 0) {
        printErrorInfo("avformat_open_input", dealwithResult);
        return;
    }
    av_dump_format(pInputFormatCtx,0,nullptr, 0);
    dealwithResult = findStreamIndex(pInputFormatCtx, AVMEDIA_TYPE_VIDEO);
    if (dealwithResult < 0) {
        //printErrorInfo("av_find_best_stream error", 0);
        return;
    }
    videoStreamIndex = dealwithResult;
    pDecodec = avcodec_find_decoder(pInputFormatCtx->streams[videoStreamIndex]->codecpar->codec_id);
    pDecodeCtx = avcodec_alloc_context3(pDecodec);
    pDecodeCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    dealwithResult = avcodec_parameters_to_context(pDecodeCtx, pInputFormatCtx->streams[videoStreamIndex]->codecpar);
    if (dealwithResult < 0) {
        //printErrorInfo("avcodec_parameters_to_context 出错", dealwithResult);
        return;
    }
    dealwithResult = avcodec_open2(pDecodeCtx, pDecodec, NULL);
    if (dealwithResult < 0) {
        //printErrorInfo("avcodec_open2 failed", dealwithResult);
        return;
    }

    if (fillVideoEncodecContainer(AV_CODEC_ID_H264) < 0) {
        return;
    }
    

    while (true) {// 循环读取packet
        dealwithResult = av_read_frame(pInputFormatCtx, pPacket);
        if (dealwithResult < 0) {  // 结束
            //printErrorInfo("av_read_frame failed", 0);
            break;
        }
        if (pPacket->stream_index != videoStreamIndex) {
            continue;
        }
        dealwithResult = avcodec_send_packet(pDecodeCtx, pPacket);
        if (dealwithResult < 0) {
            //printErrorInfo("avcodec_send_packet failed", dealwithResult);
            break;
        }

#pragma region encode and write output new video
        // 编码生成新的视频
        if (pOutFormatCtx) {
            AVPacket* pOutEncodePacket = av_packet_alloc();  // av_packet_clone(pPacket);
            av_init_packet(pPacket);
            while (true)
            {
                dealwithResult = avcodec_receive_frame(pDecodeCtx, pFrame);
                if (dealwithResult == AVERROR(EAGAIN)) {
                    break;
                }else if (dealwithResult < 0) {
                } else{
                    // 开始编码视频
                    dealwithResult = avcodec_send_frame(pOutEnecodecCtx, pFrame);
                    if (dealwithResult < 0) {
                        printErrorInfo("avcodec_send_frame Error", dealwithResult);
                    }

                    dealwithResult = avcodec_receive_packet(pOutEnecodecCtx, pOutEncodePacket);
                    if (dealwithResult < 0) {
                        if (dealwithResult == AVERROR(EAGAIN)) {
                            printErrorInfo("avcodec_receive_packet failed, must await next frame error code is ", dealwithResult);
                        } else {
                            printErrorInfo("avcodec_receive_packet Error", dealwithResult);
                        }
                    } else {
                        // 将输出pakcet写入context
                        av_write_frame(pOutFormatCtx, pOutEncodePacket);
                    }
                }
            }
        }
#pragma endregion
//         while (true) {// 循环读取frame
//             
//             dealwithResult = avcodec_receive_frame(pDecodeCtx, pFrame);
//             if (dealwithResult == AVERROR(EAGAIN)) {
//                 break;
//             } else if (dealwithResult < 0) {
//             } else {
//                 packetList.clear();
//                 AVRational rational{1, AV_TIME_BASE};
// 
//                 // 编码生成图片
//                 #pragma region mark ecode and write jpg
// //                 {
// //                     const AVCodec* pEncodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
// //                     AVCodecContext* pEncodecCtx = avcodec_alloc_context3(pEncodec);
// //                     pEncodecCtx->width = pCodeCtx->width;
// //                     pEncodecCtx->height = pCodeCtx->height;
// //                     pEncodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
// //                     pEncodecCtx->time_base = {1, 30};
// //                     pCodeCtx->framerate;
// //                     int encodeRet = avcodec_open2(pEncodecCtx, pEncodec, nullptr);
// //                     if (encodeRet < 0) {
// //                         printErrorInfo("", encodeRet);
// //                         avcodec_close(pEncodecCtx);
// //                         avcodec_free_context(&pEncodecCtx);
// //                         continue;
// //                     }
// //                     encodeRet = avcodec_send_frame(pEncodecCtx, pFrame);
// //                     if (encodeRet < 0) {
// //                         avcodec_close(pEncodecCtx);
// //                         avcodec_free_context(&pEncodecCtx);
// //                         continue;
// //                     }
// //                     
// //                     encodeRet = avcodec_receive_packet(pEncodecCtx, pEncodePacket);
// //                     if (encodeRet >= 0) {//成功
// //                         outPutFileName = "newVideo.jpg";
// //                         FILE* tempFile = fopen(outPutFileName.c_str(), "wb");
// //                         if (tempFile) {
// //                             fwrite(pEncodePacket->data, 1, pEncodePacket->size, tempFile);
// //                             fclose(tempFile);
// //                             avcodec_close(pEncodecCtx);
// //                             avcodec_free_context(&pEncodecCtx);
// //                             break;
// //                         }
// //                     }
// //                     avcodec_close(pEncodecCtx);
// //                     avcodec_free_context(&pEncodecCtx);
// //                 }
//                 #pragma endregion
// 
//             }
//         }
    }

    dealwithResult = av_write_trailer(pOutFormatCtx);
    if (dealwithResult != 0) {
        printErrorInfo("avcodec_receive_packet Error", dealwithResult);
    } else {
    }

     //av_write_trailer(pOutFormatCtx);
// 
//     avformat_close_input(&pFormatCtx);
//     avformat_free_context(pFormatCtx);
    avcodec_close(pDecodeCtx);
    avcodec_free_context(&pDecodeCtx);
     av_packet_free(&pPacket);
     av_free(pFrame);
}


void AVCombination::getFolderVedioOrAudioFilePathList(std::string rootPath, std::vector<std::string>& out_resultList, bool isAudio)
{
    if (rootPath.empty()) {
        return;
    }
#if _HAS_CXX17
	for (auto it = std::filesystem::directory_iterator(rootPath); it != std::filesystem::directory_iterator(); it++) {
        auto iterPath = it->path();
        auto itemFileName = iterPath.filename();
        if (it->is_directory()) {
            getFolderVedioOrAudioFilePathList(rootPath+"\\"+ itemFileName, out_resultList, isAudio)
        }
        else {
            auto suffixName = iterPath.extension();
            suffixName = std::tolower(suffixName);
            if ((isAudio && isAudioFileName(itemFileName)) || (!isAudio && isVideoFileName(itemFileName))) {
                out_resultList.push_back(rootPath + "\\" + itemFileName);
            }
        }        
	}

#elif _WIN64 || _WIN32  
    WIN32_FIND_DATA findedData;
    std::wstring findFolderPath;
    // string 2 wstring
    std::string folderPath = rootPath;
    folderPath += "\\*";
    int len = MultiByteToWideChar(CP_ACP, 0, folderPath.c_str(), folderPath.size(), NULL, 0);
    TCHAR* buffer = new TCHAR[len + 1];
    MultiByteToWideChar(CP_ACP, 0, folderPath.c_str(), folderPath.size(), buffer, len);
    buffer[len] = '\0';
    findFolderPath.append(buffer);

    // 遍历文件夹，找到第一个文件
    HANDLE findHandle = FindFirstFile(findFolderPath.c_str(), &findedData);

    if (findHandle != INVALID_HANDLE_VALUE) {
        do 
        {
            // wchar 2 string
            auto nameCharLen = WideCharToMultiByte(CP_OEMCP, 0, findedData.cFileName, wcslen(findedData.cFileName),
                                                   NULL, 0, NULL, NULL);
            char* itemFileName = new char[nameCharLen + 1];
            memset(itemFileName, '\0', nameCharLen + 1);
            WideCharToMultiByte(CP_OEMCP, 0, findedData.cFileName, wcslen(findedData.cFileName), itemFileName,
                                nameCharLen, NULL, NULL);

            std::string fileNameStr{itemFileName};
            if (fileNameStr != "." && fileNameStr != "..") {
                std::string itemFilePath = rootPath + "\\" + fileNameStr;
                if (findedData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {  // 文件夹

                    getFolderVedioOrAudioFilePathList(itemFilePath, out_resultList, isAudio);

                } else {
                    if ((isAudio && isAudioFileName(itemFileName)) || (!isAudio && isVideoFileName(itemFileName))) {
                        out_resultList.push_back(itemFilePath);
                    }
                }
            }

        } while ( FindNextFile(findHandle, &findedData) );
    }
#elif __unix__ || __linux__ // linux unix 下的操作
	DIR* pDir;
	struct dirent* ptr;
	if (!(pDir = opendir(rootPath.c_str()))) {
		return;
	}
	while ((ptr = readdir(pDir)) != 0) {
		if (strcmp(ptr->d_name, ".") == 0 && strcmp(ptr->d_name, "..") == 0) {
			continue;
		}
		std::string itemPath = rootPath + "\\" + ptr->d_name;
		if (ptr->d_type == DT_DIR) {
			getFolderVedioOrAudioFilePathList(itemPath, out_resultList, isAudio)
		}
        if ((isAudio && isAudioFileName(ptr->d_name)) || (!isAudio && isVideoFileName(ptr->d_name))) {
            out_resultList.push_back(itemPath);
        }
	}
	closedir(pDir);
#endif
}
#pragma endregion

#pragma region private method
void AVCombination::scaleWHFrame(AVFrame* pDesFrame) {
    if (!pDesFrame) {
        return;
    }
    pVideoSwsCtx = sws_getCachedContext(pVideoSwsCtx, pDesFrame->width, pDesFrame->height, AV_PIX_FMT_YUV420P,
                                        targetVideoWidth, targetVideoHeight, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!pVideoOutFrame) {
        pVideoOutFrame = av_frame_alloc();
        pVideoOutFrame->format = AV_PIX_FMT_YUV420P;
        pVideoOutFrame->width = targetVideoWidth;
        pVideoOutFrame->height = targetVideoHeight;
        av_frame_get_buffer(pVideoOutFrame, 0);
    }
    int ret = sws_scale(pVideoSwsCtx, pDesFrame->data, pDesFrame->linesize, 0, pDesFrame->height, pVideoOutFrame->data,
                        pVideoOutFrame->linesize);
    pVideoOutFrame->pts = pDesFrame->pkt_dts;
    if (ret<0) {
        printErrorInfo("sws_scale video error", ret);
    }
}

int AVCombination::findStreamIndex(AVFormatContext* pSrcCtx, AVMediaType desType) {
    if (!pSrcCtx) {
        printErrorInfo("findDesCodecIndex error src format context is null", 0);
        return -1;
    }
    int result = av_find_best_stream(pSrcCtx, desType, -1, -1, NULL, 0);
    if (result >= 0) {
        return result;
    }
    for (size_t streamIndex = 0; streamIndex < pSrcCtx->nb_streams; streamIndex++) {
        AVStream* pItemStream = pSrcCtx->streams[streamIndex];   
        if (!pItemStream) {
            continue;
        }
        if (pItemStream->codecpar->codec_type == desType) {
            return streamIndex;
        }
    }
    return -1;
}

void AVCombination::fillStartCode(AVCodecParameters* pSrcCodecParam, AVBSFContext** pBsfCtx) {
    if (nullptr == *pBsfCtx) {
        const AVBitStreamFilter* bsfilter = av_bsf_get_by_name("h264_mp4toannexb");
        // 2 初始化过滤器上下文
        av_bsf_alloc(bsfilter, pBsfCtx);  // AVBSFContext;
        // 3 添加解码器属性
        avcodec_parameters_copy((*pBsfCtx)->par_in, pSrcCodecParam);
        av_bsf_init(*pBsfCtx);
    }
}

int AVCombination::fillVideoEncodecContainer(AVCodecID codeId) {
    pOutCodec = avcodec_find_encoder(codeId);
    avcodec_close(pOutEnecodecCtx);
    avcodec_free_context(&pOutEnecodecCtx);
    pOutEnecodecCtx = avcodec_alloc_context3(pOutCodec);

    avformat_free_context(pOutFormatCtx);
    pOutFormatCtx = avformat_alloc_context();

    pOutputFormat = av_guess_format(nullptr, outPutFileName.c_str(), nullptr);
    pOutFormatCtx->oformat = pOutputFormat;
    avformat_alloc_output_context2(&pOutFormatCtx, pOutputFormat, nullptr, outPutFileName.c_str());  // 视频流操作

    int ret = avio_open(&(pOutFormatCtx->pb), outPutFileName.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        printErrorInfo("open out stream error", ret);
        return ret;
    }

    AVStream* pEncodeStream = avformat_new_stream(pOutFormatCtx, pOutCodec);
    if (!pEncodeStream) {
        return -1;
    }
    pEncodeStream->time_base = {0, 1};

    pOutEnecodecCtx->codec = pOutCodec;
    // 编码类型
    pOutEnecodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    // 注意：这个类型是根据你解码的时候指定的解码的视频像素数据格式类型
    pOutEnecodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    // 视频宽高
    pOutEnecodecCtx->width = pDecodeCtx->width;
    pOutEnecodecCtx->height = pDecodeCtx->height;
    // 编码率 视频大小/视频时间
    pOutEnecodecCtx->bit_rate = pDecodeCtx->bit_rate;
    // gop画面帧大小 gopsize数量之后插入一个I帧（关键帧）越少视频就越小但过分少会导致编码失败
    pOutEnecodecCtx->gop_size = pDecodeCtx->gop_size;
    // 设置帧率 这里是每秒30帧，帧数越大越流畅
    pOutEnecodecCtx->time_base = {1, 30};
    pOutEnecodecCtx->framerate = {0, 1};
    // 设置量化参数
    pOutEnecodecCtx->qmin = pDecodeCtx->qmin;
    pOutEnecodecCtx->qmax = pDecodeCtx->qmax;
    // 设置B帧最大值 前后预测帧
    pOutEnecodecCtx->max_b_frames = pDecodeCtx->max_b_frames;
    ret = avcodec_parameters_from_context(pEncodeStream->codecpar, pOutEnecodecCtx);

    // 若是H264编码器，要设置一些参数
    AVDictionary* param = NULL;
    if (pOutEnecodecCtx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "slow", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    // 设置H264的编码器参数为延迟模式，提高编码质量，但是会造成编码速度下降
    // av_opt_set(pOutDecodecCtx->priv_data, "preset", "slow", 0);
    ret = avcodec_open2(pOutEnecodecCtx, NULL, &param);
    if (ret < 0) {
        printErrorInfo("encode context open error", ret);
        avcodec_close(pOutEnecodecCtx);
        avcodec_free_context(&pOutEnecodecCtx);
        pOutEnecodecCtx = nullptr;
        return ret;
    }
    pOutFormatCtx->streams[0]->codecpar;
    ret = avformat_write_header(pOutFormatCtx, nullptr);
    if (ret < 0) {
        avcodec_close(pOutEnecodecCtx);
        avcodec_free_context(&pOutEnecodecCtx);
        pOutEnecodecCtx = nullptr;
        printErrorInfo("avformat_write_header Error", ret);
        return ret;
    }
    return 0;
}

void AVCombination::outputFile(AVPacket* pDesPacket) {

}

void AVCombination::dealloc() {
}

void AVCombination::printErrorInfo(std::string prefixStr, int errorCode) {
    char error[1024];
    if (errorCode != 0) {
        av_strerror(errorCode, error, 1024);
        std::cout << prefixStr << " " << errorCode << ", Msg content: " << error << std::endl;
        return;
    }
    std::cout << prefixStr << " " << errorCode<< std::endl;
}
#pragma endregion

