#import <Cocoa/Cocoa.h>
#import "../src/space_explorer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

@interface STSpaceResult : NSObject { @public STSpaceNode *tree; int error; }
@end
@interface STSpaceExplorer (TestAccess)
- (void)launchWorker:(NSString *)path;
- (void)finished:(id)result;
- (void)selectPath:(NSString *)path;
@end
static int completed, destroyed;
static pthread_t mainThread;
@interface TestExplorer : STSpaceExplorer
- (void)startClosedScan:(NSString *)path;
@end
@implementation TestExplorer
- (void)startClosedScan:(NSString *)path
{
    scanning = YES; closed = YES;
    [self retain];
    [self launchWorker:path];
}
- (void)finished:(id)result
{
    assert(pthread_equal(pthread_self(), mainThread));
    assert(!((STSpaceResult *)result)->error && ((STSpaceResult *)result)->tree);
    assert(scanning && !root && closed);
    completed = 1;
    [super finished:result];
}
- (void)dealloc { destroyed = 1; [super dealloc]; }
@end

static int openCompleted, openDestroyed, workerWaiting, allowWorkerExit, workerReleased;
static pthread_mutex_t workerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t workerCondition = PTHREAD_COND_INITIALIZER;
/* Exercise the real job while keeping the NSThread target alive after delivery. */
@interface STSpaceScanJob : NSObject { STSpaceExplorer *explorer; }
- (id)initWithExplorer:(STSpaceExplorer *)value;
- (void)run:(NSString *)path;
@end
@interface PausedScanJob : STSpaceScanJob
@end
@implementation PausedScanJob
- (void)run:(NSString *)path
{
    [super run:path];
    pthread_mutex_lock(&workerMutex);
    workerWaiting = 1;
    pthread_cond_signal(&workerCondition);
    while (!allowWorkerExit) pthread_cond_wait(&workerCondition, &workerMutex);
    workerReleased = 1;
    pthread_cond_signal(&workerCondition);
    pthread_mutex_unlock(&workerMutex);
}
@end
@interface OpenWorkerExplorer : STSpaceExplorer
- (NSTimer *)timer;
@end
@implementation OpenWorkerExplorer
- (void)buildWindow { }
- (NSTimer *)timer { return refreshTimer; }
- (void)launchWorker:(NSString *)path
{
    PausedScanJob *job = [[PausedScanJob alloc] initWithExplorer:self];
    [NSThread detachNewThreadSelector:@selector(run:) toTarget:job withObject:path];
    [job release];
}
- (void)finished:(id)result
{
    assert(pthread_equal(pthread_self(), mainThread));
    assert(!openDestroyed && scanning && !closed);
    assert(!((STSpaceResult *)result)->error && ((STSpaceResult *)result)->tree);
    openCompleted = 1;
    [super finished:result];
}
- (void)dealloc { assert(pthread_equal(pthread_self(), mainThread)); assert(openCompleted); openDestroyed = 1; [super dealloc]; }
@end
int main(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    const char *tmp = getenv("TMPDIR");
    char path[2048];
    NSString *directory;
    TestExplorer *explorer;
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:10];
    mainThread = pthread_self();
    assert(tmp && snprintf(path, sizeof(path), "%s/space-thread-XXXXXX", tmp) > 0);
    assert(mkdtemp(path));
    directory = [NSString stringWithUTF8String:path];
    explorer = [[TestExplorer alloc] init];
    [explorer startClosedScan:directory];
    [explorer release];
    explorer = nil;
    while ((!completed || !destroyed) && [deadline timeIntervalSinceNow] > 0)
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    assert(completed && destroyed);
    {
        NSAutoreleasePool *inner = [[NSAutoreleasePool alloc] init];
        OpenWorkerExplorer *open = [[OpenWorkerExplorer alloc] init];
        NSTimer *timer;
        struct timespec limit;
        limit.tv_sec = time(NULL) + 10; limit.tv_nsec = 0;
        [open selectPath:directory];
        timer = [[open timer] retain]; assert(timer && [timer isValid]);
        [open release];
        assert(!openCompleted && !openDestroyed);
        pthread_mutex_lock(&workerMutex);
        while (!workerWaiting) assert(pthread_cond_timedwait(&workerCondition, &workerMutex, &limit) == 0);
        pthread_mutex_unlock(&workerMutex);
        deadline = [NSDate dateWithTimeIntervalSinceNow:10];
        while (!openCompleted && [deadline timeIntervalSinceNow] > 0)
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
        assert(openCompleted);
        [inner drain];
        assert(openDestroyed && ![timer isValid]);
        pthread_mutex_lock(&workerMutex);
        allowWorkerExit = 1;
        pthread_cond_signal(&workerCondition);
        limit.tv_sec = time(NULL) + 10;
        while (!workerReleased) assert(pthread_cond_timedwait(&workerCondition, &workerMutex, &limit) == 0);
        pthread_mutex_unlock(&workerMutex);
        [timer release];
    }
    assert(!rmdir(path));
    [pool drain];
    puts("PASS background scan delivers on main thread and discards closed result");
    return 0;
}
