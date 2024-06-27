#include <jni.h>
#include <string>
#include <x264.h>
#include <rtmp.h>
#include "VideoChannel.h"
#include "AudioChannel.h"
#include "util.h"
#include "safe_queue.h"
#include "client/linux/handler/minidump_descriptor.h"
#include "client/linux/handler/exception_handler.h"

VideoChannel *videoChannel = nullptr;
AudioChannel *audioChannel = nullptr;
bool isStart;
pthread_t pid_start;
bool readyPushing;
SafeQueue<RTMPPacket *> packets;
uint32_t start_time;

void releasePackets(RTMPPacket **packet) {
    if (packet) {
        RTMPPacket_Free(*packet);
        delete *packet;
        *packet = nullptr;
    }
}

// 存放packet到队列
void callback(RTMPPacket *packet) {
    if (packet) {
        if (packet->m_nTimeStamp == -1) {
            packet->m_nTimeStamp = RTMP_GetTime() - start_time;
        }
        packets.push(packet);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myrtmp_MyPusher_native_1init(JNIEnv *env, jobject thiz) {
    // C++层的初始化工作
    videoChannel = new VideoChannel();
    audioChannel = new AudioChannel();

    // 存入队列的关联
    videoChannel->setVideoCallback(callback);
    audioChannel->setAudioCallback(callback);

    // 队列的释放工作关联
    packets.setReleaseCallback(releasePackets);
}

void *task_start(void *args) {
    char *url = static_cast<char *>(args);
    RTMP *rtmp = nullptr;
    int ret;

    do {
        // 1.1，rtmp 分配内存
        rtmp = RTMP_Alloc();
        if (!rtmp) {
            LOGE("rtmp 分配内存失败");
            break;
        }

        // 1.2，rtmp 初始化
        RTMP_Init(rtmp);
        rtmp->Link.timeout = 5; // 设置连接的超时时间（以秒为单位的连接超时）

        // 2，rtmp 设置流媒体地址
        ret = RTMP_SetupURL(rtmp, url);
        if (!ret) { // ret == 0 和 ffmpeg不同，0代表失败
            LOGE("rtmp 设置流媒体地址失败");
            break;
        }

        // 3，开启输出模式
        RTMP_EnableWrite(rtmp);

        // 4，建立连接
        ret = RTMP_Connect(rtmp, nullptr);
        if (!ret) { // ret == 0 和 ffmpeg不同，0代表失败
            LOGE("rtmp 建立连接失败:%d, url: %s", ret, url);
            break;
        }

        // 5，连接流
        ret = RTMP_ConnectStream(rtmp, 5);
        if (ret == FALSE) { // ret == 0 和 ffmpeg不同，0代表失败
            LOGE("rtmp 连接流失败");
            break;
        }

        start_time = RTMP_GetTime();

        readyPushing = true;

        // 队列开始工作
        packets.setWork(1);

        // 发送音频编码器的解码配置信息
        callback(audioChannel->getAudioSeqHeader());

        RTMPPacket *packet = nullptr;

        LOGE("rtmp 开始推流");

        while (readyPushing) {
            packets.pop(packet); // 阻塞式

            if (!readyPushing) {
                break;
            }

            if (!packet) {
                continue;
            }

            packet->m_nInfoField2 = rtmp->m_stream_id;

            ret = RTMP_SendPacket(rtmp, packet, 1); // 1==true 开启内部缓冲

            releasePackets(&packet);

            if (!ret) { // ret == 0 和 ffmpeg不同，0代表失败
                LOGE("rtmp 失败 自动断开服务器");
                break;
            }
        }
    } while (false);

    isStart = false;
    readyPushing = false;
    packets.setWork(0);
    packets.clear();

    if (rtmp) {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
    }
    delete[] url;

    return nullptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myrtmp_MyPusher_native_1start(JNIEnv *env, jobject thiz, jstring path_) {
    // 子线程  1.连接流媒体服务器， 2.发包
    if (isStart) {
        return;
    }

    isStart = true;
    const char *path = env->GetStringUTFChars(path_, nullptr);

    char *url = new char[strlen(path) + 1]; // C++的堆区开辟 new -- delete
    strcpy(url, path);

    pthread_create(&pid_start, nullptr, task_start, url);

    env->ReleaseStringUTFChars(path_, path);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myrtmp_MyPusher_native_1stop(JNIEnv *env, jobject thiz) {
    isStart = false;
    readyPushing = false;
    packets.setWork(0);
    pthread_detach(pid_start);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myrtmp_MyPusher_native_1release(JNIEnv *env, jobject thiz) {
    DELETE(videoChannel);
    DELETE(audioChannel);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myrtmp_MyPusher_native_1initVideoEncoder(JNIEnv *env, jobject thiz, jint width,
                                                          jint height, jint m_fps, jint bitrate) {
    if (videoChannel) {
        videoChannel->initVideoEncoder(width, height, m_fps, bitrate);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myrtmp_MyPusher_native_1pushVideo(JNIEnv *env, jobject thiz, jbyteArray data_) {
    // data == nv21数据  编码 加入队列
    if (!videoChannel || !readyPushing) { return; }
    jbyte *data = env->GetByteArrayElements(data_, nullptr);
    videoChannel->encodeData(data);
    env->ReleaseByteArrayElements(data_, data, 0); // 释放byte[]
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myrtmp_MyPusher_native_1initAudioEncoder(JNIEnv *env, jobject thiz,
                                                          jlong sample_rate, jint num_channels) {
    if (audioChannel) {
        audioChannel->initAudioEncoder(sample_rate, num_channels);
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_myrtmp_MyPusher_native_1getInputSamples(JNIEnv *env, jobject thiz) {
    if (audioChannel) {
        return audioChannel->getInputSamples();
    }
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myrtmp_MyPusher_native_1pushAudio(JNIEnv *env, jobject thiz, jbyteArray data_) {
    if (!audioChannel || !readyPushing) {
        return;
    }
    jbyte *data = env->GetByteArrayElements(data_, nullptr);

    jsize length = env->GetArrayLength(data_);
    int32_t *intArray = (int32_t *) malloc(length * sizeof(int32_t));
    for (jsize i = 0; i < length; i++) {
        intArray[i] = (int32_t) data[i];
    }

    audioChannel->encodeData(intArray);

    env->ReleaseByteArrayElements(data_, data, 0);
}

bool DumpCallback(const google_breakpad::MinidumpDescriptor &descriptor,
                  void *context,
                  bool succeeded) {
    LOGE("native crash:%s", descriptor.path());
    return false;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_myrtmp_crash_CrashReport_initNativeCrash(JNIEnv *env, jclass clazz,
                                                          jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);

    google_breakpad::MinidumpDescriptor descriptor(path);
    static google_breakpad::ExceptionHandler eh(descriptor, NULL, DumpCallback,
                                                NULL, true, -1);
    env->ReleaseStringUTFChars(path_, path);
}