#ifndef MYRTMP_AUDIOCHANNEL_H
#define MYRTMP_AUDIOCHANNEL_H

#include <faac.h>
#include <sys/types.h>
#include <rtmp.h>
#include <cstring>
#include "util.h"
#include <pthread.h>
#include <malloc.h>

class AudioChannel {
public:
    typedef void (*AudioCallback)(RTMPPacket *packet);

    AudioChannel();

    ~AudioChannel();

    void initAudioEncoder(unsigned long sample_rate, unsigned int channels);

    int getInputSamples();

    void encodeData(int32_t *data);

    void setAudioCallback(AudioCallback audioCallback);

    RTMPPacket *getAudioSeqHeader();

private:
    pthread_mutex_t mutexAudio;
    unsigned long inputSamples; // faac 输入的样本数
    unsigned long maxOutputBytes; // faac 编码器最大能输出的字节数
    unsigned int mChannels = 2; // 通道数
    unsigned char *buffer = nullptr; // 编码后的输出 buffer
    faacEncHandle audioEncoder = nullptr; // 音频编码器
    AudioCallback audioCallback{};
};


#endif
