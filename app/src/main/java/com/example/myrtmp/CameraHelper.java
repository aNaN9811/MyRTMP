package com.example.myrtmp;

import android.app.Activity;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

import androidx.annotation.NonNull;

import java.util.Iterator;
import java.util.List;

public class CameraHelper implements SurfaceHolder.Callback, Camera.PreviewCallback {

    private static final String TAG = "CameraHelper";
    private final Activity mActivity;
    private int mHeight; // 高
    private int mWidth; // 宽
    private int mCameraId; // 后摄 前摄像头
    private Camera mCamera; // Camera1 预览采集图像数据
    private byte[] buffer; // 数据
    private SurfaceHolder mSurfaceHolder; // Surface画面的帮助
    private Camera.PreviewCallback mPreviewCallback; // 后面预览的画面，把此预览的画面 的数据回调出现 ---> MyPush ---> C++层
    private OnChangedSizeListener mOnChangedSizeListener; // 你的宽和高发生改变，就会回调此接口

    // 构造 必须传递基本上参数
    public CameraHelper(Activity activity, int cameraId, int width, int height) {
        mActivity = activity;
        mCameraId = cameraId;
        mWidth = width;
        mHeight = height;
    }

    /**
     * 开始预览
     */
    private void startPreview() {
        try {
            // 获得camera对象
            mCamera = Camera.open(mCameraId);
            // 配置camera的属性
            Camera.Parameters parameters = mCamera.getParameters();
            // 设置预览数据格式为nv21
            parameters.setPreviewFormat(ImageFormat.NV21); // yuv420类型的子集
            // 这是摄像头宽、高
            setPreviewSize(parameters);
            // 设置摄像头 图像传感器的角度、方向
            setPreviewOrientation();
            mCamera.setParameters(parameters);
            buffer = new byte[mWidth * mHeight * 3 / 2];
            // 数据缓存区
            mCamera.addCallbackBuffer(buffer);
            mCamera.setPreviewCallbackWithBuffer(this);
            // 设置预览画面
            mCamera.setPreviewDisplay(mSurfaceHolder); // SurfaceView 和 Camera绑定
            if (mOnChangedSizeListener != null) { // 你的宽和高发生改变，就会回调此接口
                mOnChangedSizeListener.initVideoEncoder(mWidth, mHeight);
            }
            // 开启预览
            mCamera.startPreview();
        } catch (Exception ex) {
            ex.printStackTrace();
        }
    }

    /**
     * 切换摄像头
     */
    public void switchCamera() {
        if (mCameraId == Camera.CameraInfo.CAMERA_FACING_BACK) {
            mCameraId = Camera.CameraInfo.CAMERA_FACING_FRONT;
        } else {
            mCameraId = Camera.CameraInfo.CAMERA_FACING_BACK;
        }
        stopPreview(); // 先停止预览
        startPreview(); // 在开启预览
    }

    /**
     * 停止预览
     */
    public void stopPreview() {
        if (mCamera != null) {
            // 预览数据回调接口
            mCamera.setPreviewCallback(null);
            // 停止预览
            mCamera.stopPreview();
            // 释放摄像头
            mCamera.release();
            mCamera = null;
        }
    }

    /**
     * 与Surface绑定 == surfaceView.getHolder()
     * @param surfaceHolder
     */
    public void setPreviewDisplay(SurfaceHolder surfaceHolder) {
        mSurfaceHolder = surfaceHolder;
        mSurfaceHolder.addCallback(this);
        startPreview();
    }

    /**
     * 旋转画面角度（因为默认预览是歪的，所以就需要旋转画面角度）
     * 这个只是画面的旋转，但是数据不会旋转，你还需要额外处理
     */
    private void setPreviewOrientation() {
        Camera.CameraInfo info = new Camera.CameraInfo();
        Camera.getCameraInfo(mCameraId, info);
        // 旋转画面相关的标识
        int mRotation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
        int degrees = 0;
        switch (mRotation) {
            case Surface.ROTATION_0:
                degrees = 0;
                break;
            case Surface.ROTATION_90: // 横屏 左边是头部(home键在右边)
                degrees = 90;
                break;
            case Surface.ROTATION_180:
                degrees = 180;
                break;
            case Surface.ROTATION_270:// 横屏 头部在右边
                degrees = 270;
                break;
        }
        int result;
        if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
            result = (info.orientation + degrees) % 360;
            result = (360 - result) % 360; // compensate the mirror
        } else { // back-facing
            result = (info.orientation - degrees + 360) % 360;
        }
        // 设置角度
        mCamera.setDisplayOrientation(result);
    }

    /**
     * 在设置宽和高的同时，能够打印 支持的分辨率
     * @param parameters
     */
    private void setPreviewSize(Camera.Parameters parameters) {
        // 获取摄像头支持的宽、高
        List<Camera.Size> supportedPreviewSizes = parameters.getSupportedPreviewSizes();
        Camera.Size size = supportedPreviewSizes.get(0);
        Log.i(TAG, "Camera支持: " + size.width + "x" + size.height);
        // 选择一个与设置的差距最小的支持分辨率
        int m = Math.abs(size.height * size.width - mWidth * mHeight);
        supportedPreviewSizes.remove(0);
        Iterator<Camera.Size> iterator = supportedPreviewSizes.iterator();
        while (iterator.hasNext()) {
            Camera.Size next = iterator.next();
            Log.i(TAG, "支持 " + next.width + "x" + next.height);
            int n = Math.abs(next.height * next.width - mWidth * mHeight);
            if (n < m) {
                m = n;
                size = next;
            }
        }
        mWidth = size.width;
        mHeight = size.height;
        parameters.setPreviewSize(mWidth, mHeight);
        Log.i(TAG, "预览分辨率 width:" + size.width + " height:" + size.height);
    }


    // ========================================= 回调实现 ===========================================

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder holder) {
        Log.d(TAG, "surfaceCreated: ");
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
        Log.d(TAG, "surfaceChanged: ");
        mSurfaceHolder = holder;
        // 释放摄像头
        stopPreview();
        // 开启摄像头
        startPreview();
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {

    }

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        if (mPreviewCallback != null) {
            mPreviewCallback.onPreviewFrame(data, camera); // byte[] data == nv21 ===> C++层 ---> 流媒体服务器
        }
        camera.addCallbackBuffer(buffer);
    }

    public void setPreviewCallback(Camera.PreviewCallback previewCallback) {
        mPreviewCallback = previewCallback;
    }

    public void setOnChangedSizeListener(OnChangedSizeListener listener) {
        mOnChangedSizeListener = listener;
    }

    public interface OnChangedSizeListener {
        void initVideoEncoder(int width, int height);
    }
}
