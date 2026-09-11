package org.edp17.snapszer;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.IBinder;

// Keeps a LAN game reachable while Snapszer is hosting, joining or playing.
// Android silently drops incoming packets for apps that are in the background
// or considered idle; a foreground service exempts the app, and the Wi-Fi
// lock keeps the radio from sleeping between moves.
public class LanService extends Service
{
    private static final String CHANNEL = "lan";
    private WifiManager.WifiLock m_wifiLock;

    public static void start(Context context)
    {
        Intent intent = new Intent(context, LanService.class);
        if (Build.VERSION.SDK_INT >= 26)
            context.startForegroundService(intent);
        else
            context.startService(intent);
    }

    public static void stop(Context context)
    {
        context.stopService(new Intent(context, LanService.class));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId)
    {
        Notification.Builder builder;
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            manager.createNotificationChannel(
                    new NotificationChannel(CHANNEL, "LAN game", NotificationManager.IMPORTANCE_LOW));
            builder = new Notification.Builder(this, CHANNEL);
        } else {
            builder = new Notification.Builder(this);
        }
        Intent open = getPackageManager().getLaunchIntentForPackage(getPackageName());
        PendingIntent pending = PendingIntent.getActivity(this, 0, open,
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification notification = builder
                .setContentTitle("Snapszer")
                .setContentText("LAN game active")
                .setSmallIcon(getApplicationInfo().icon)
                .setContentIntent(pending)
                .setOngoing(true)
                .build();
        if (Build.VERSION.SDK_INT >= 34)
            startForeground(1, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else
            startForeground(1, notification);

        if (m_wifiLock == null) {
            WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            if (wifi != null) {
                @SuppressWarnings("deprecation")
                int mode = WifiManager.WIFI_MODE_FULL_HIGH_PERF;
                m_wifiLock = wifi.createWifiLock(mode, "snapszer-lan");
                m_wifiLock.setReferenceCounted(false);
                m_wifiLock.acquire();
            }
        }
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy()
    {
        if (m_wifiLock != null) {
            m_wifiLock.release();
            m_wifiLock = null;
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent)
    {
        return null;
    }
}
