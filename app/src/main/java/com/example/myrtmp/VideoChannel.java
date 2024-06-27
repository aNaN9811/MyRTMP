package com.example.myrtmp;

import android.app.Activity;
import android.hardware.Camera;
import android.view.SurfaceHolder;

public class VideoChannel implements Camera.PreviewCallback, CameraHelper.OnChangedSizeListener {

    private CameraHelper cameraHelper; // Camera画面预览帮助类
    private int bitrate; // 码率
    private int mFps; // fps
    private boolean isLive; // 是否直播，通过此标记控制是否发送数据给C++层
    private MyPusher mPusher; // 把Camera画面数据 回调给中转站，其他数据 回调给中转站，中转站和C++层打交道

    public VideoChannel(MyPusher pusher, Activity activity, int cameraId, int width, int height, int fps, int bitrate) {
        this.mPusher = pusher; // 回调给总部：中转站
        this.mFps = fps; // fps 每秒钟多少帧
        this.bitrate = bitrate; // 码率
        cameraHelper = new CameraHelper(activity, cameraId, width, height); // Camera相机预览帮助类
        cameraHelper.setPreviewCallback(this); // 设置Camera相机预览帮助类，onPreviewFrame(nv21)数据的回调监听
        cameraHelper.setOnChangedSizeListener(this); // 宽高发送改变的监听回调设置
    }

    // 与Surface绑定 == surfaceView.getHolder()
    public void setPreviewDisplay(SurfaceHolder holder) {
        cameraHelper.setPreviewDisplay(holder);
    }

    // 切换摄像头
    public void switchCamera() {
        cameraHelper.switchCamera();
    }

    // 开始直播，只修改标记 让其可以进入if 完成图像数据推送
    public void startLive() {
        isLive = true;
    }

    // 停止直播，只修改标记 让其可以不要进入if 就不会再数据推送了
    public void stopLive() {
        isLive = false;
    }

    // Camera预览画面的数据<--调用帮助类
    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        // data == nv21 数据
        if (isLive) {
            // 图像数据推送
            mPusher.native_pushVideo(data);
        }
    }

    // 初始化编码器宽高改变
    @Override
    public void initVideoEncoder(int width, int height) {
        // 视频编码器的初始化有关：width，height，fps，bitrate
        mPusher.native_initVideoEncoder(width, height, mFps, bitrate); // 初始化x264编码器
    }

    // 调用帮助类-->停止预览
    public void release() {
        cameraHelper.stopPreview();
    }
}
