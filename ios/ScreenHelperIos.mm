// The one thing ScreenHelper does on iOS: keep the display awake while a LAN
// game is hosted, joined or played. Everything else it does on Android -- the
// foreground service, the Wi-Fi lock -- has no counterpart here, because an
// iOS app in the background loses its sockets regardless.

#import <UIKit/UIKit.h>

void snapszer_keepScreenOn(bool on)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [UIApplication sharedApplication].idleTimerDisabled = on ? YES : NO;
    });
}
