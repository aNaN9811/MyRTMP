# MyRTMP

通过交叉编译 arm64-v8a 的 x264 和 FAAC 实现视频及音频原始数据的编码，

引入 RTMPDump 实现编码后的音视频数据封装成 RTMP 协议的数据包并发送到相应的服务器完成直播的推流。

同时引入 BreakPad 实现对于 Native 层 Crash 数据的收集。