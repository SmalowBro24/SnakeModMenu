package com.snakemod;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

public final class OverlayBootstrap {
    private OverlayBootstrap() {}

    public static void start(Context context) {
        try {
            // Register this library with the app ClassLoader so native methods resolve.
            System.loadLibrary("snakemod");
        } catch (UnsatisfiedLinkError ignored) {
            // It may already be registered by the linker/class loader.
        }
        if (Settings.canDrawOverlays(context)) {
            context.startService(new Intent(context, OverlayService.class));
        } else {
            Intent intent = new Intent(context, PermissionActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
        }
    }
}
