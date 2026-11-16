#import <Cocoa/Cocoa.h>
#import "timer_support.h"

void st_schedule_update_timer(NSTimer *timer)
{
    NSRunLoop *loop = [NSRunLoop currentRunLoop];
    [loop addTimer:timer forMode:NSDefaultRunLoopMode];
    [loop addTimer:timer forMode:NSEventTrackingRunLoopMode];
}
