#import <Cocoa/Cocoa.h>
#import "../src/space_explorer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

@interface STSpaceExplorer (TestAccess)
- (void)selectPath:(NSString *)path;
- (void)refresh:(id)sender;
- (void)finished:(id)result;
- (void)windowWillClose:(NSNotification *)note;
@end
static int completions;
static pthread_t mainThread;
@interface LiveExplorer : STSpaceExplorer
- (uint64_t)bytes;
@end
@implementation LiveExplorer
- (void)buildWindow { }
- (void)startRefreshTimer { }
- (void)stopRefreshTimer { }
- (uint64_t)bytes { assert(root); return root->bytes; }
- (void)finished:(id)result
{
    assert(pthread_equal(pthread_self(), mainThread));
    [super finished:result]; ++completions;
}
@end
static void waitFor(int count)
{
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:10];
    while (completions < count && [deadline timeIntervalSinceNow] > 0)
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    assert(completions == count);
}
int main(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    const char *tmp = getenv("TMPDIR");
    char dir[2048], file[2100];
    int fd;
    LiveExplorer *e;
    mainThread = pthread_self();
    assert(tmp && snprintf(dir, sizeof(dir), "%s/space-refresh-XXXXXX", tmp) > 0);
    assert(mkdtemp(dir));
    assert(snprintf(file, sizeof(file), "%s/item", dir) > 0);
    fd = open(file, O_WRONLY | O_CREAT | O_EXCL, 0600); assert(fd >= 0);
    assert(write(fd, "abc", 3) == 3 && !close(fd));
    e = [[LiveExplorer alloc] init];
    [e selectPath:[NSString stringWithUTF8String:dir]]; waitFor(1); assert([e bytes] == 3);
    fd = open(file, O_WRONLY | O_APPEND); assert(fd >= 0);
    assert(write(fd, "defgh", 5) == 5 && !close(fd));
    [e refresh:nil]; waitFor(2); assert([e bytes] == 8);
    [e windowWillClose:nil]; [e release];
    assert(!unlink(file) && !rmdir(dir));
    [pool drain]; puts("PASS real background refresh sees changed file size"); return 0;
}
