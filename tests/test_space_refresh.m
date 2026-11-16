#import <Cocoa/Cocoa.h>
#import "../src/space_explorer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

@interface STSpaceResult : NSObject { @public STSpaceNode *tree; int error; }
@end
@interface STSpaceExplorer (RefreshTestAccess)
- (void)selectPath:(NSString *)path;
- (void)refresh:(id)sender;
- (void)refreshTick:(NSTimer *)timer;
- (void)startRefreshTimer;
- (void)stopRefreshTimer;
- (void)launchWorker:(NSString *)path;
- (void)finished:(STSpaceResult *)result;
- (void)windowWillClose:(NSNotification *)note;
@end
static int launches, timers, stops, destroyed;
@interface FakeLabel : NSObject { NSString *value; }
- (void)setStringValue:(NSString *)s;
- (NSString *)stringValue;
@end
@implementation FakeLabel
- (void)setStringValue:(NSString *)s { [value release]; value = [s copy]; }
- (NSString *)stringValue { return value; }
- (void)dealloc { [value release]; [super dealloc]; }
@end
@interface FakeExplorer : STSpaceExplorer
- (void)deliver:(int)error name:(const char *)name;
- (int)busy;
- (const STSpaceNode *)tree;
- (void)modal:(BOOL)value;
- (size_t)depth;
- (void)navigate;
- (NSString *)message;
@end
@implementation FakeExplorer
- (void)buildWindow { if (!status) status = (NSTextField *)[[FakeLabel alloc] init]; }
- (NSString *)message { return [(FakeLabel *)status stringValue]; }
- (void)startRefreshTimer { ++timers; }
- (void)stopRefreshTimer { ++stops; }
- (void)launchWorker:(NSString *)path { assert(path); ++launches; }
- (void)deliver:(int)error name:(const char *)name
{
    STSpaceResult *r = [[STSpaceResult alloc] init];
    r->error = error;
    if (!error) {
        r->tree = calloc(1, sizeof(*r->tree)); assert(r->tree);
        r->tree->name = strdup(name); assert(r->tree->name);
        r->tree->flags = ST_SPACE_DIRECTORY;
        if (!strcmp(name, "partial")) r->tree->flags |= ST_SPACE_INCOMPLETE;
        r->tree->children = calloc(1, sizeof(*r->tree->children)); assert(r->tree->children);
        r->tree->children[0] = calloc(1, sizeof(*r->tree)); assert(r->tree->children[0]);
        r->tree->children[0]->name = strdup("child"); assert(r->tree->children[0]->name);
        r->tree->children[0]->flags = ST_SPACE_DIRECTORY; r->tree->child_count = 1;
    }
    [self finished:r]; [r release];
}
- (int)busy { return scanning; }
- (const STSpaceNode *)tree { return root; }
- (void)modal:(BOOL)value { [self setValue:[NSNumber numberWithBool:value] forKey:@"choosing"]; }
- (size_t)depth { return navigation.depth; }
- (void)navigate { assert(st_space_nav_enter(&navigation, 0)); }
- (void)dealloc { ++destroyed; [super dealloc]; }
@end
static int timerLaunches, timerDestroyed;
@interface TimerExplorer : STSpaceExplorer
- (NSTimer *)timer;
- (void)complete;
@end
@implementation TimerExplorer
- (void)buildWindow { }
- (void)launchWorker:(NSString *)path { assert(path); ++timerLaunches; }
- (NSTimer *)timer { return refreshTimer; }
- (void)complete { STSpaceResult *r = [[STSpaceResult alloc] init]; r->error = EACCES; [self finished:r]; [r release]; }
- (void)dealloc { ++timerDestroyed; [super dealloc]; }
@end
static void runTimerMode(TimerExplorer *e, NSString *mode)
{
    int before = timerLaunches;
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:1];
    [[e timer] setFireDate:[NSDate dateWithTimeIntervalSinceNow:-1]];
    while (timerLaunches == before && [deadline timeIntervalSinceNow] > 0)
        [[NSRunLoop currentRunLoop] runMode:mode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    assert(timerLaunches == before + 1);
    [e complete];
}
int main(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    FakeExplorer *e = [[FakeExplorer alloc] init];
    const STSpaceNode *old;
    [e selectPath:@"/first"];
    assert(launches == 1 && timers == 1 && [e busy]);
    [e refresh:nil]; [e refreshTick:nil]; assert(launches == 1);
    [e deliver:0 name:"first"]; assert(![e busy] && [e tree]);
    assert([[e message] rangeOfString:@"Logical size: 0 B"].location != NSNotFound);
    [e navigate]; assert([e depth] == 1);
    [e refresh:nil]; assert(launches == 2 && [e busy]);
    old = [e tree]; [e deliver:EACCES name:NULL]; assert([e tree] == old && ![e busy]);
    assert([[e message] rangeOfString:@"Refresh failed"].location != NSNotFound);
    [e modal:YES]; [e refreshTick:nil]; [e refresh:nil]; assert(launches == 2);
    [e modal:NO]; [e refreshTick:nil]; assert(launches == 3 && [e busy] && [e tree] == old);
    assert([[e message] rangeOfString:@"Refresh failed"].location != NSNotFound);
    assert([[e message] rangeOfString:@"Refreshing in background"].location != NSNotFound);
    [e deliver:0 name:"partial"]; assert([e depth] == 0 && !strcmp([e tree]->name, "partial"));
    assert([[e message] rangeOfString:@"Refresh failed"].location == NSNotFound);
    assert([[e message] rangeOfString:@"SCAN INCOMPLETE"].location != NSNotFound);
    [e refreshTick:nil]; assert(launches == 4);
    [e windowWillClose:nil]; assert(stops == 1 && ![e tree]);
    [e refresh:nil]; [e refreshTick:nil]; assert(launches == 4);
    [e selectPath:@"/second"]; assert(launches == 4 && timers == 2);
    [e deliver:0 name:"stale"]; assert(launches == 5 && [e busy] && ![e tree]);
    [e deliver:0 name:"second"]; assert(![e busy] && !strcmp([e tree]->name, "second"));
    [e refresh:nil]; assert(launches == 6);
    [e windowWillClose:nil];
    [e deliver:0 name:"closed"]; assert(![e busy] && ![e tree] && launches == 6);
    [e release]; assert(destroyed == 1);
    {
        TimerExplorer *t = [[TimerExplorer alloc] init];
        NSTimer *timer;
        [t selectPath:@"/timer"]; assert(timerLaunches == 1);
        timer = [[t timer] retain]; assert([timer timeInterval] == 30.0 && [timer isValid]);
        [t complete];
        runTimerMode(t, NSDefaultRunLoopMode);
        runTimerMode(t, NSEventTrackingRunLoopMode);
        [t windowWillClose:nil]; assert(![timer isValid]);
        [timer release]; [t release];
    }
    {
        NSAutoreleasePool *inner = [[NSAutoreleasePool alloc] init];
        TimerExplorer *t = [[TimerExplorer alloc] init];
        NSTimer *timer;
        int before = timerDestroyed;
        [t selectPath:@"/owner-release"];
        timer = [[t timer] retain]; assert(timer && [timer isValid]);
        [t complete]; /* release the independent worker ownership */
        [t release];
        [inner drain];
        assert(timerDestroyed == before + 1 && ![timer isValid]);
        [timer release];
    }
    [pool drain]; puts("PASS refresh scheduling, overlap, errors, close and stale result"); return 0;
}
