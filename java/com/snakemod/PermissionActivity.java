package com.snakemod;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.Toast;

public final class PermissionActivity extends Activity {
    private boolean openedSettings = false;
    private boolean launched = false;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (Settings.canDrawOverlays(this)) {
            startOverlay();
            return;
        }
        if (!openedSettings) {
            openedSettings = true;
            Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
            startActivityForResult(intent, 71);
        }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 71) {
            if (Settings.canDrawOverlays(this)) startOverlay();
            else {
                Toast.makeText(this, "Разрешение поверх окон не выдано — меню отключено", Toast.LENGTH_LONG).show();
                finish();
            }
        }
    }

    @Override protected void onResume() {
        super.onResume();
        if (Build.VERSION.SDK_INT >= 23 && Settings.canDrawOverlays(this) && openedSettings) startOverlay();
    }

    private void startOverlay() {
        if (launched) return;
        launched = true;
        startService(new Intent(this, OverlayService.class));
        finish();
    }
}
