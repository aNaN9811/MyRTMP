package com.example.myrtmp;

import android.annotation.SuppressLint;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class AudioChannel {
    private final MyPusher mPusher;
    private boolean isLive; // 是否直播：开始直播就是true，停止直播就是false，通过此标记控制是否发送数据给C++层
    private AudioRecord audioRecord; // AudioRecord采集Android麦克风音频数据 --> C++层 --> 编码 --> 封包 --> 加入队列
    private final ExecutorService executorService;
    int inputSamples; // 样本数 4096

    @SuppressLint("MissingPermission")
    public AudioChannel(MyPusher pusher) {
        this.mPusher = pusher;
        executorService = Executors.newSingleThreadExecutor();
        mPusher.native_initAudioEncoder(44100, 2);
        inputSamples = mPusher.getInputSamples() * 2;
        int minBufferSize = AudioRecord.getMinBufferSize(44100,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT);
        audioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC,
                44100,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                Math.max(inputSamples, minBufferSize));
    }

    public void startLive() {
        isLive = true;
        executorService.submit(new AudioTask());
    }

    // 停止直播，只修改标记 让其可以不要进入while 就不会再数据推送了
    public void stopLive() {
        isLive = false;
    }

    // AudioRecord的释放工作
    public void release() {
        if (audioRecord != null) {
            audioRecord.release();
            audioRecord = null;
        }
    }

    // 子线程：AudioRecord采集录制音频数据，再把此数据传递给 --> C++层(进行编码) --> 封包(RTMPPacket) --> 发送
    private class AudioTask implements Runnable {
        @Override
        public void run() {
            audioRecord.startRecording();
            byte[] bytes = new byte[inputSamples];
            while (isLive) {
                int len = audioRecord.read(bytes, 0, bytes.length);
                if (len > 0) {
                    mPusher.native_pushAudio(bytes);
                }
            }
            audioRecord.stop();
        }
    }
}
