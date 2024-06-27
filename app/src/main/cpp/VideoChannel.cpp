#include "VideoChannel.h"

VideoChannel::VideoChannel() {
    pthread_mutex_init(&mutex, 0);
}

VideoChannel::~VideoChannel() {
    pthread_mutex_destroy(&mutex);
}

void VideoChannel::initVideoEncoder(int width, int height, int fps, int bitrate) {
    // 防止编码器多次创建 互斥锁
    pthread_mutex_lock(&mutex);

    mWidth = width;
    mHeight = height;
    mFps = fps;
    mBitrate = bitrate;

    y_len = width * height;
    uv_len = y_len / 4;

    // 防止重复初始化
    if (videoEncoder) {
        x264_encoder_close(videoEncoder);
        videoEncoder = nullptr;
    }
    if (pic_in) {
        x264_picture_clean(pic_in);
        DELETE(pic_in)
    }

    x264_param_t param;

    // 设置编码器属性
    x264_param_default_preset(&param, "ultrafast", "zerolatency");

    // 编码规格：https://wikipedia.tw.wjbk.site/wiki/H.264
    param.i_level_idc = 32; // 3.2 中等偏上的规格  自动用 码率，模糊程度，分辨率

    param.i_csp = X264_CSP_I420;
    param.i_width = width;
    param.i_height = height;

    // 不能有B帧，如果有B帧会影响编码、解码效率
    param.i_bframe = 0;

    // 码率控制方式。CQP(恒定质量)，CRF(恒定码率)，ABR(平均码率)
    param.rc.i_rc_method = X264_RC_CRF;

    // 设置码率
    param.rc.i_bitrate = bitrate / 1000;

    // 瞬时最大码率 网络波动导致的
    param.rc.i_vbv_max_bitrate = bitrate / 1000 * 1.2;

    // 设置了i_vbv_max_bitrate就必须设置buffer大小，码率控制区大小，单位Kb/s
    param.rc.i_vbv_buffer_size = bitrate / 1000;

    // 码率控制不是通过 timebase 和 timestamp
    param.b_vfr_input = 0;

    // 帧率分子
    param.i_fps_num = fps;
    // 帧率分母表示 1s
    param.i_fps_den = 1;

    // 时间基（timebase）用于描述时间戳的精度，这个设置的结果是每个时间单位表示一帧的时间
    param.i_timebase_den = param.i_fps_num;
    param.i_timebase_num = param.i_fps_den;

    // 帧距离(关键帧)  2s一个关键帧   （就是把两秒钟一个关键帧告诉人家）
    param.i_keyint_max = fps * 2;

    // sps序列参数   pps图像参数集，所以需要设置header(sps pps)
    // 是否复制sps和pps放在每个关键帧的前面 该参数设置是让每个关键帧(I帧)都附带sps/pps。
    param.b_repeat_headers = 1;

    // 并行编码线程数
    param.i_threads = 1;

    x264_param_apply_profile(&param, "baseline");

    pic_in = new x264_picture_t;
    x264_picture_alloc(pic_in, param.i_csp, param.i_width, param.i_height);

    videoEncoder = x264_encoder_open(&param);
    if (videoEncoder) {
        LOGE("x264编码器打开成功");
    }

    pthread_mutex_unlock(&mutex);
}

void VideoChannel::encodeData(signed char *data) {
    pthread_mutex_lock(&mutex);

    // 把 nv21 的y分量转成 i420 的y分量
    memcpy(pic_in->img.plane[0], data, y_len);

    for (int i = 0; i < uv_len; ++i) {
        // u 数据
        // data + y_len + i * 2 + 1 : 移动指针取 data(nv21) 中 u 的数据
        *(pic_in->img.plane[1] + i) = *(data + y_len + i * 2 + 1);

        // v 数据
        // data + y_len + i * 2 ： 移动指针取 data(nv21) 中 v 的数据
        *(pic_in->img.plane[2] + i) = *(data + y_len + i * 2);
    }

    x264_nal_t *nal = nullptr; // 通过H.264编码得到NAL数组
    int pi_nal; // pi_nal是nal中输出的NAL单元的数量
    x264_picture_t pic_out; // 输出编码后图片 （编码后的图片）

    // 1.视频编码器，
    // 2.nal，
    // 3.pi_nal是nal中输出的NAL单元的数量，
    // 4.输入原始的图片，
    // 5.输出编码后图片
    int ret = x264_encoder_encode(videoEncoder, &nal, &pi_nal, pic_in,
                                  &pic_out);
    if (ret < 0) { // 返回值：x264_encoder_encode函数 返回返回的 NAL 中的字节数。如果没有返回 NAL 单元，则在错误时返回负数和零。
        LOGE("x264编码失败");
        pthread_mutex_unlock(&mutex); // 编码失败解锁，否则有概率性造成死锁了
        return;
    }

    // 发送 Packets 入队queue
    int sps_len, pps_len; // sps 和 pps 的长度
    uint8_t sps[100]; // 用于接收 sps 的数组定义
    uint8_t pps[100]; // 用于接收 pps 的数组定义
    pic_in->i_pts += 1; // pts显示的时间（+=1 目的是每次都累加下去）， dts编码的时间

    for (int i = 0; i < pi_nal; ++i) {
        if (nal[i].i_type == NAL_SPS) {
            sps_len = nal[i].i_payload - 4; // 去掉起始码
            memcpy(sps, nal[i].p_payload + 4, sps_len); // 由于上面减了4，所以+4挪动这里的位置开始
        } else if (nal[i].i_type == NAL_PPS) {
            pps_len = nal[i].i_payload - 4; // 去掉起始码
            memcpy(pps, nal[i].p_payload + 4, pps_len); // 由于上面减了4，所以+4挪动这里的位置开始

            // sps + pps
            sendSpsPps(sps, pps, sps_len, pps_len); // pps是跟在sps后面的，这里拿到的pps表示前面的sps肯定拿到了
        } else {
            // 发送 I帧 P帧
            sendFrame(nal[i].i_type, nal[i].i_payload, nal[i].p_payload);
        }
    }

    pthread_mutex_unlock(&mutex);
}

void VideoChannel::sendSpsPps(uint8_t *sps, uint8_t *pps, int sps_len, int pps_len) {
    int body_size = 5 + 8 + sps_len + 3 + pps_len;

    RTMPPacket *packet = new RTMPPacket;

    RTMPPacket_Alloc(packet, body_size);

    int i = 0;
    packet->m_body[i++] = 0x17;

    packet->m_body[i++] = 0x00; // 如果是1 帧类型（关键帧 非关键帧）， 如果是0一定是 sps pps
    packet->m_body[i++] = 0x00;
    packet->m_body[i++] = 0x00;
    packet->m_body[i++] = 0x00;

    packet->m_body[i++] = 0x01; // 版本

    packet->m_body[i++] = sps[1];
    packet->m_body[i++] = sps[2];
    packet->m_body[i++] = sps[3];

    packet->m_body[i++] = 0xFF;
    packet->m_body[i++] = 0xE1;

    // 一个字节表达一个长度，需要位移
    // 一个字节表达 sps的长度，所以就需要位运算，取出sps_len高8位 再取出sps_len低8位
    packet->m_body[i++] = (sps_len >> 8) & 0xFF; // 取高8位
    packet->m_body[i++] = sps_len & 0xFF; // 去低8位

    memcpy(&packet->m_body[i], sps, sps_len); // sps拷贝进去了

    i += sps_len; // 拷贝完pps数据 ，i移位，（下面才能准确移位）

    packet->m_body[i++] = 0x01; // pps个数，用一个字节表示

    // 一个字节表达一个长度，需要位移
    // 一个字节表达 pps的长度，所以就需要位运算，取出pps_len高8位 再取出pps_len低8位
    packet->m_body[i++] = (pps_len >> 8) & 0xFF; // 取高8位
    packet->m_body[i++] = pps_len & 0xFF; // 去低8位

    memcpy(&packet->m_body[i], pps, pps_len);

    // 封包处理
    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packet->m_nBodySize = body_size; // 设置好 sps+pps的总大小
    packet->m_nChannel = 0x10; // 通道ID，随便写一个，注意：不要写的和rtmp.c(里面的m_nChannel有冲突 4301行)
    packet->m_nTimeStamp = 0; // sps pps 包 没有时间戳
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;

    // packet 存入队列
    videoCallback(packet);
}

void VideoChannel::setVideoCallback(VideoCallback callback) {
    this->videoCallback = callback;
}

void VideoChannel::sendFrame(int type, int payload, uint8_t *pPayload) {
    // 去掉起始码 00 00 00 01 或者 00 00 01
    if (pPayload[2] == 0x00) { // 00 00 00 01
        pPayload += 4; // 例如：共10个，挪动4个后，还剩6个
        // 保证 我们的长度是和上的数据对应，也要是6个，所以-= 4
        payload -= 4;
    } else if (pPayload[2] == 0x01) { // 00 00 01
        pPayload += 3; // 例如：共10个，挪动3个后，还剩7个
        // 保证长度是和上的数据对应，也要是7个，所以-= 3
        payload -= 3;
    }

    int body_size = 5 + 4 + payload;

    RTMPPacket *packet = new RTMPPacket;

    RTMPPacket_Alloc(packet, body_size);

    // 区分关键帧 和 非关键帧
    packet->m_body[0] = 0x27; // 普通帧 非关键帧
    if (type == NAL_SLICE_IDR) {
        packet->m_body[0] = 0x17; // 关键帧
    }

    packet->m_body[1] = 0x01; // 如果是1 帧类型（关键帧 非关键帧）， 如果是0一定是 sps pps
    packet->m_body[2] = 0x00;
    packet->m_body[3] = 0x00;
    packet->m_body[4] = 0x00;

    packet->m_body[5] = (payload >> 24) & 0xFF;
    packet->m_body[6] = (payload >> 16) & 0xFF;
    packet->m_body[7] = (payload >> 8) & 0xFF;
    packet->m_body[8] = payload & 0xFF;

    memcpy(&packet->m_body[9], pPayload, payload); // 拷贝H264的裸数据

    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO; // 包类型，是视频类型
    packet->m_nBodySize = body_size; // 设置好 关键帧 或 普通帧 的总大小
    packet->m_nChannel = 0x10; // 注意：不要写的和rtmp.c(里面的m_nChannel有冲突 4301行)
    packet->m_nTimeStamp = -1; // 帧数据有时间戳
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;

    // 把最终的 帧类型 RTMPPacket 存入队列
    videoCallback(packet);
}