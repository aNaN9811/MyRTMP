package com.example.myrtmp;

import android.app.Activity;
import android.view.SurfaceHolder;

public class MyPusher {

    static {
        System.loadLibrary("myrtmp");
    }

    private final VideoChannel videoChannel;
    private final AudioChannel audioChannel;

    // ①:初始化native层需要的加载，
    // ②:实例化视频通道并传递基本参数(宽高,fps,码率等)，
    // ③:实例化视频通道
    public MyPusher(Activity activity, int cameraId, int width, int height, int fps, int bitrate) {
        native_init();
        videoChannel = new VideoChannel(this, activity, cameraId, width, height, fps, bitrate);
        audioChannel = new AudioChannel(this);
    }

    /**
     * 开始直播
     *
     * @param path rtmp地址
     */
    public void startLive(String path) {
        videoChannel.startLive();
        audioChannel.startLive();
        native_start(path);
    }

    /**
     * 停止直播
     */
    public void stopLive() {
        videoChannel.stopLive();
        audioChannel.stopLive();
        native_stop();
    }

    /**
     * 释放工作
     */
    public void release() {
        videoChannel.release();
        audioChannel.release();
        native_release();
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 视频通道 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    // SurfaceView与中转站里面的Camera绑定
    public void setPreviewDisplay(SurfaceHolder holder) {
        videoChannel.setPreviewDisplay(holder);
    }

    // 切换摄像头
    public void switchCamera() {
        videoChannel.switchCamera();
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 音频通道 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    // 音频通道需要样本数（faac的编码器，输出样本 的样本数，才是标准）
    public int getInputSamples() {
        return native_getInputSamples(); // native层-->从faacEncOpen中获取到的样本数
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> native函数 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    // 音频 视频 公用的
    public native void native_init(); // 初始化

    public native void native_start(String path); // 开始直播start(音频视频通用一套代码) path:rtmp推流地址

    public native void native_stop(); // 停止直播

    public native void native_release(); // onDestroy--->release释放工作

    // 视频独有
    public native void native_initVideoEncoder(int width, int height, int mFps, int bitrate); // 初始化x264编码器

    public native void native_pushVideo(byte[] data); // 相机画面的数据 byte[] 推给 C++层

    // 音频独有
    public native void native_initAudioEncoder(long sampleRate, int numChannels); // 初始化faac音频编码器

    public native int native_getInputSamples(); // 获取facc编码器 样本数

    public native void native_pushAudio(byte[] bytes); // 把audioRecord采集的原始数据，给C++层编码 --> 入队 --> 发给流媒体服务器
}
