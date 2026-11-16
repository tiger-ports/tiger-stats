#import <Cocoa/Cocoa.h>
#include <stdio.h>

#import "../src/timer_support.h"

@interface Counter : NSObject { @public int ticks; }
- (void)tick:(NSTimer *)timer;
@end
@implementation Counter
- (void)tick:(NSTimer *)timer { (void)timer; ++ticks; }
@end

static int expect_timer_in_mode(NSString *mode, Counter *counter)
{
    int before = counter->ticks;
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:0.5];
    while (counter->ticks == before && [deadline timeIntervalSinceNow] > 0) {
        [[NSRunLoop currentRunLoop] runMode:mode beforeDate:deadline];
    }
    if (counter->ticks == before) {
        fprintf(stderr, "FAIL timer did not fire in %s\n", [mode UTF8String]);
        return 1;
    }
    return 0;
}

int main(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    Counter *counter = [[Counter alloc] init];
    NSTimer *timer = [NSTimer timerWithTimeInterval:0.01 target:counter selector:@selector(tick:) userInfo:nil repeats:YES];
    int result;
    st_schedule_update_timer(timer);
    result = expect_timer_in_mode(NSDefaultRunLoopMode, counter);
    if (!result) result = expect_timer_in_mode(NSEventTrackingRunLoopMode, counter);
    [timer invalidate];
    [counter release];
    [pool drain];
    if (!result) puts("PASS timer in default and menu-tracking modes");
    return result;
}
