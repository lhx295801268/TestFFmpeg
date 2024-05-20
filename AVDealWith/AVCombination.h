#pragma once
#include <iostream>
#include <vector>
#include <string>
extern "C" {
    #include "libavutil/avutil.h"
    #include "libswscale/swscale.h"
    #include "libavcodec/avcodec.h"
    #include "libavcodec/bsf.h"
    #include "libavdevice/avdevice.h"
    #include "libavfilter/avfilter.h"
    #include "libavformat/avformat.h"
    #include "libavformat/internal.h"
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
    /*****************
 * Creator:
 * Date:
 * Brief:
 *******************/
class AVCombination
{
#pragma region public attribute
public:
    int targetVideoWidth = 720;
    int targetVideoHeight = 1280;
    std::string outPutFileName = ".\\newMedia.mp4";
#pragma endregion

#pragma region public method
public:
    AVCombination();
    ~AVCombination();
    bool mergeVedio(std::string filePath1, std::string filePath2);
    void decodeVideo(std::string filePath);
#pragma endregion

#pragma region private method
private:
    /**
     * @brief : scaleWHFrame 尺寸转换
     * @param : pDesFrame
     * @return:
     * @author: TsunamiLee
     **/
    void scaleWHFrame(AVFrame* pDesFrame);

    /**
     * @brief : reEncodecVideo 重新编码函数
     * @param : packetList, pDesFrame
     * @return:
     * @author: TsunamiLee
     **/
    void reEncodecVideo(std::vector<AVPacket*>& packetList, AVFrame* pDesFrame, AVCodecContext* pOutCodecCtx);

    /**
     * @brief : findDesCodecIndex 在上下文中找到对应媒体类型的流索引
     * @param : pSrcCtx, 已经获取好的上下文
     * @param : desType 目标流媒体类型
     * @return: 小于0：获取失败 大于等于0：流媒体索引
     * @author: TsunamiLee
     **/
    int findStreamIndex(AVFormatContext* pSrcCtx, AVMediaType desType);

    /**
     * @brief : fillStartCode mp4解析出来的流没有startcode 不能直接用于播放，这里需要手动添加
     * @param : pSrcCodecParam 编码参数
     * @param : pBsfCtx bsf 上下文
     * @return:
     * @author: TsunamiLee
     **/
    void fillStartCode(AVCodecParameters* pSrcCodecParam, AVBSFContext** pBsfCtx);

    int fillVideoEncodecContainer(AVCodecID codeId);

    void outputFile(AVPacket* pDesPacket);

    /**
     * @brief : 本地类释放函数
     * @author: TsunamiLee
     **/
    void dealloc();

    void printErrorInfo(std::string prefixStr, int errorCode);
#pragma endregion

#pragma region private attribute
private:
    AVFormatContext* pInputFormatCtx = nullptr;
    AVCodecContext* pDecodeCtx = nullptr;
    const AVCodec* pDecodec = nullptr;
    const AVOutputFormat* pOutputFormat = nullptr;

    AVFormatContext* pOutFormatCtx = nullptr;
    AVBSFContext* pBSFCtx = nullptr;
    const AVCodec* pOutCodec = nullptr;
    AVCodecContext* pOutEnecodecCtx = nullptr;
    SwsContext* pVideoSwsCtx = nullptr;
    AVFrame* pVideoOutFrame = nullptr;
    FILE* pOutVideoFile = nullptr;
#pragma endregion
};
