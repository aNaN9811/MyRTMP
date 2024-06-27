package com.example.myrtmp;

import android.Manifest;
import android.content.pm.PackageManager;
import android.hardware.Camera;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.example.myrtmp.crash.CrashReport;
import com.example.myrtmp.databinding.ActivityMainBinding;

import java.util.Arrays;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";
    private ActivityMainBinding binding;

    private MyPusher pusher;
    private SurfaceView surfaceView;
    private static final int PERMISSION_CODE = 100;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        CrashReport.init(this);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        surfaceView = findViewById(R.id.surfaceView);

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
                != PackageManager.PERMISSION_GRANTED) {
            // 权限尚未授予，请求权限
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO}, PERMISSION_CODE);
        } else {
            // 权限已授予，可以使用相机
            startCamera();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        Log.d(TAG, "onRequestPermissionsResult: " + requestCode);

        if (requestCode == PERMISSION_CODE) {
            Log.d(TAG, "onRequestPermissionsResult: " + Arrays.toString(permissions));
            if (grantResults.length > 0 &&
                    grantResults[0] == PackageManager.PERMISSION_GRANTED &&
                    grantResults[1] == PackageManager.PERMISSION_GRANTED) {
                // 权限被授予，可以使用相机
                startCamera();
            } else {
                // 权限被拒绝，显示一个提示
                Toast.makeText(this, "直播需要相机和麦克风的权限", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private void startCamera() {
        Log.d(TAG, "startCamera: ");
        pusher = new MyPusher(this, Camera.CameraInfo.CAMERA_FACING_FRONT, 1080, 1920,25, 8000_000);
        pusher.setPreviewDisplay(surfaceView.getHolder());
    }

    /**
     * 切换摄像头
     *
     * @param view
     */
    public void switchCamera(View view) {
        pusher.switchCamera();
    }

    /**
     * 开始直播
     *F
     * @param view
     */
    public void startLive(View view) {
        pusher.startLive("");
    }

    /**
     * 停止直播
     *
     * @param view
     */
    public void stopLive(View view) {
        pusher.stopLive();
    }

    @Override
    protected void onDestroy() {
        pusher.release();
        super.onDestroy();
    }
}