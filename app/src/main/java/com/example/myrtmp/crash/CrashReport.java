package com.example.myrtmp.crash;

import android.content.Context;

import java.io.File;

public class CrashReport {

    static {
        System.loadLibrary("myrtmp");
    }

    public static void init(Context context) {
        Context applicationContext = context.getApplicationContext();
        CrashHandler.init(applicationContext);
        File file = new File(applicationContext.getExternalCacheDir(), "native_crash");
        if (!file.exists()) {
            file.mkdirs();
        }
        initNativeCrash(file.getAbsolutePath());
    }


    private static native void initNativeCrash(String path);
}
