#import <Cocoa/Cocoa.h>
#import "../src/space_explorer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
extern NSString *STSpaceExactPath(const char *s);
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < 1050
typedef int TestTag;
#else
typedef NSInteger TestTag;
#endif
@interface STSpaceExplorer (TrashTestAccess)
- (void)moveToTrash:(id)sender;
- (void)refresh:(id)sender;
- (void)refreshTick:(NSTimer *)timer;
- (void)windowWillClose:(NSNotification *)note;
- (void)startScan;
@end
static int selectedCount = 1;
@interface FakeTable : NSObject
- (TestTag)selectedRow;
- (TestTag)numberOfSelectedRows;
- (void)reloadData;
- (void)setDataSource:(id)value;
- (void)setDelegate:(id)value;
@end
@implementation FakeTable
- (TestTag)selectedRow { return 0; }
- (TestTag)numberOfSelectedRows { return selectedCount; }
- (void)reloadData { }
- (void)setDataSource:(id)value { (void)value; }
- (void)setDelegate:(id)value { (void)value; }
@end
static int calls, confirms, scans, confirmAnswer = 1, closeDuring, tickDuring, replaceDuring, tickWorkspace;
static char fixtureFile[2048], fixtureOld[2048], fixtureDir[2048];
static BOOL workspaceAnswer = YES;
static TestTag workspaceTag = 0;
static STSpaceExplorer *activeExplorer;
@interface FakeWorkspace : NSObject
- (BOOL)performFileOperation:(NSString *)operation source:(NSString *)source destination:(NSString *)destination files:(NSArray *)files tag:(TestTag *)tag;
@end
@implementation FakeWorkspace
- (BOOL)performFileOperation:(NSString *)operation source:(NSString *)source destination:(NSString *)destination files:(NSArray *)files tag:(TestTag *)tag
{
    assert([operation isEqualToString:NSWorkspaceRecycleOperation]);
    assert([source isEqualToString:[NSString stringWithUTF8String:fixtureDir]]);
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < 1060
    assert([destination isEqualToString:@""]);
#else
    assert(destination == nil);
#endif
    assert([files count] == 1 && [[files objectAtIndex:0] isEqualToString:@"file"]);
    if (tickWorkspace) [activeExplorer refresh:nil];
    ++calls; *tag = workspaceTag; return workspaceAnswer;
}
@end
@interface FakeStatus : NSObject { NSString *value; }
- (void)setStringValue:(NSString *)s;
- (NSString *)stringValue;
@end
@implementation FakeStatus
- (void)setStringValue:(NSString *)s { [value release]; value = [s copy]; }
- (NSString *)stringValue { return value; }
- (void)dealloc { [value release]; [super dealloc]; }
@end
@interface FakeChart : NSObject { const STSpaceNode *displayed; }
- (void)setNode:(const STSpaceNode *)node;
- (const STSpaceNode *)node;
@end
@implementation FakeChart
- (void)setNode:(const STSpaceNode *)node { displayed = node; }
- (const STSpaceNode *)node { return displayed; }
@end
@interface FakeTrashExplorer : STSpaceExplorer
- (void)prepare:(STSpaceNode *)tree;
- (int)pending;
- (void)busy:(BOOL)value;
- (NSString *)message;
- (void)useRealRefresh;
- (BOOL)busyState;
- (BOOL)chartShowsTree;
- (uint64_t)treeBytes;
@end
@implementation FakeTrashExplorer
- (void)prepare:(STSpaceNode *)tree
{
    root = tree; closed = NO; selectedPath = [[NSString alloc] initWithUTF8String:tree->name];
    st_space_nav_init(&navigation, root); table = (NSTableView *)[[FakeTable alloc] init];
    status = (NSTextField *)[[FakeStatus alloc] init];
}
- (BOOL)confirmTrash:(NSString *)path
{
    ++confirms; assert([path isEqualToString:[NSString stringWithUTF8String:fixtureFile]]);
    if (tickDuring) [self refreshTick:nil];
    if (closeDuring) [self windowWillClose:nil];
    if (replaceDuring) {
        FILE *f;
        assert(!rename(fixtureFile, fixtureOld));
        f = fopen(fixtureFile, "w"); assert(f && !fclose(f));
    }
    return confirmAnswer;
}
- (NSWorkspace *)trashWorkspace { activeExplorer = self; return (NSWorkspace *)[[[FakeWorkspace alloc] init] autorelease]; }
- (void)startScan { if (chart) [super startScan]; else ++scans; }
- (void)busy:(BOOL)value { scanning = value; }
- (int)pending { return [[self valueForKey:@"trashPending"] boolValue]; }
- (NSString *)message { return [(FakeStatus *)status stringValue]; }
- (void)useRealRefresh { chart = (NSView *)[[FakeChart alloc] init]; }
- (BOOL)busyState { return scanning; }
- (BOOL)chartShowsTree { return root && [(FakeChart *)chart node] == root; }
- (uint64_t)treeBytes { assert(root); return root->bytes; }
@end
static STSpaceNode *scan(const char *dir) { STSpaceNode *n; assert(!st_space_scan(dir, NULL, &n)); return n; }
@interface STSpaceResult : NSObject { @public STSpaceNode *tree; int error; }
@end
@interface STSpaceExplorer (SyntheticCompletion)
- (void)finished:(STSpaceResult *)result;
@end
static int launches;
static NSString *const warning = @"Trash operation may still be pending; completion unverified. Chart is provisional; further Trash disabled.";
@interface PendingExplorer : FakeTrashExplorer
- (void)deliver:(int)error;
- (NSTimer *)timer;
- (BOOL)hasTree;
@end
@implementation PendingExplorer
- (void)launchWorker:(NSString *)path { assert([path isEqualToString:[NSString stringWithUTF8String:fixtureDir]]); ++launches; }
- (void)deliver:(int)error
{
    STSpaceResult *result = [[STSpaceResult alloc] init];
    result->error = error;
    if (!error) result->tree = scan(fixtureDir);
    [self finished:result]; [result release];
}
- (NSTimer *)timer { return refreshTimer; }
- (BOOL)hasTree { return root != NULL; }
@end
static void pendingRefreshTests(void)
{
    PendingExplorer *e = [[PendingExplorer alloc] init];
    int beforeConfirm;
    NSTimer *timer;
    [e prepare:scan(fixtureDir)]; [e useRealRefresh];
    workspaceAnswer = YES; workspaceTag = 42;
    [e moveToTrash:nil];
    assert([e pending] && launches == 1 && [e busyState]);
    assert([[e message] rangeOfString:warning].location != NSNotFound);
    beforeConfirm = confirms;
    [e moveToTrash:nil]; [e refresh:nil]; [e refreshTick:nil];
    assert(confirms == beforeConfirm && calls == 1 && launches == 1);
    [e deliver:0];
    assert([e chartShowsTree] && [e pending]);
    assert([[e message] rangeOfString:warning].location != NSNotFound);
    [e moveToTrash:nil]; assert(confirms == beforeConfirm && calls == 1);
    assert(!unlink(fixtureFile));
    [e refresh:nil]; assert(launches == 2 && [e busyState]);
    assert([[e message] rangeOfString:warning].location != NSNotFound);
    [e refreshTick:nil]; assert(launches == 2);
    [e deliver:0]; assert([e pending] && [e chartShowsTree]);
    assert([[e message] rangeOfString:warning].location != NSNotFound);
    {
        FILE *file = fopen(fixtureFile, "w"); assert(file && !fclose(file));
    }
    [e refresh:nil]; assert(launches == 3);
    [e deliver:EACCES];
    assert([e chartShowsTree] && [[e message] rangeOfString:warning].location != NSNotFound);
    assert([[e message] rangeOfString:@"Refresh failed"].location != NSNotFound);
    [e refreshTick:nil]; assert(launches == 4);
    [e windowWillClose:nil]; [e refresh:nil]; [e refreshTick:nil]; assert(launches == 4);
    [e explore:nil]; timer = [e timer];
    assert(timer && [timer isValid] && launches == 4);
    assert([[e message] rangeOfString:warning].location != NSNotFound);
    [e deliver:0]; assert(launches == 5 && [e busyState] && ![e hasTree]);
    [e deliver:0]; assert([e chartShowsTree] && [e pending]);
    assert([[e message] rangeOfString:warning].location != NSNotFound);
    [e moveToTrash:nil]; assert(confirms == beforeConfirm && calls == 1);
    [timer setFireDate:[NSDate dateWithTimeIntervalSinceNow:-1]];
    {
        NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:1];
        while (launches == 5 && [deadline timeIntervalSinceNow] > 0)
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    assert(launches == 6 && [e busyState]);
    [e windowWillClose:nil]; [e deliver:0];
    assert(![e hasTree] && launches == 6);
    [e explore:nil]; assert(launches == 7 && [e busyState]);
    [e deliver:0]; assert([e chartShowsTree] && [e pending]);
    [e windowWillClose:nil]; [e release];
    e = [[PendingExplorer alloc] init]; [e prepare:scan(fixtureDir)]; [e useRealRefresh];
    workspaceAnswer = NO; workspaceTag = 7; launches = 0;
    [e moveToTrash:nil]; assert([e pending] && launches == 1);
    assert([[e message] rangeOfString:warning].location != NSNotFound);
    [e deliver:EACCES]; assert([e pending] && [[e message] rangeOfString:warning].location != NSNotFound);
    [e moveToTrash:nil]; assert(calls == 2);
    [e windowWillClose:nil]; [e release];
    calls = confirms = scans = launches = 0; workspaceAnswer = YES; workspaceTag = 0;
}
int main(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    const char *tmp = getenv("TMPDIR"); char dir[2048], path[2048]; FILE *f;
    FakeTrashExplorer *e;
    assert([STSpaceExactPath("/tmp/file") isEqualToString:@"/tmp/file"]);
    assert(STSpaceExactPath("invalid-\xff") == nil);
    {
        const char *name = "caf\xc3\xa9";
        NSString *candidate = [NSString stringWithUTF8String:name];
        const char *roundtrip = [candidate fileSystemRepresentation];
        assert((STSpaceExactPath(name) != nil) == (roundtrip && !strcmp(name, roundtrip)));
    }
    assert(tmp && snprintf(dir, sizeof(dir), "%s/trash-ui-XXXXXX", tmp) > 0 && mkdtemp(dir));
    strcpy(fixtureDir, dir);
    assert(snprintf(path, sizeof(path), "%s/file", dir) > 0);
    strcpy(fixtureFile, path); assert(snprintf(fixtureOld, sizeof(fixtureOld), "%s/old", dir) > 0);
    f = fopen(path, "w"); assert(f && !fclose(f));
    pendingRefreshTests();
    e = [[FakeTrashExplorer alloc] init]; [e prepare:scan(dir)];
    selectedCount = 2; [e moveToTrash:nil]; assert(!confirms && !calls); selectedCount = 1;
    [e busy:YES]; [e moveToTrash:nil]; assert(!confirms && !calls); [e busy:NO];
    confirmAnswer = 0; [e moveToTrash:nil]; assert(confirms == 1 && calls == 0);
    confirmAnswer = 1; tickDuring = 1; tickWorkspace = 1;
    [e moveToTrash:nil]; assert(confirms == 2 && calls == 1 && scans == 1);
    assert([[e message] rangeOfString:@"completed"].location != NSNotFound);
    [e release];
    e = [[FakeTrashExplorer alloc] init]; [e prepare:scan(dir)];
    workspaceTag = 42; [e moveToTrash:nil]; assert(calls == 2 && [e pending]);
    [e moveToTrash:nil]; [e refresh:nil]; assert(calls == 2 && scans == 3);
    [e windowWillClose:nil]; [e moveToTrash:nil]; assert(calls == 2);
    [e explore:nil]; [e refreshTick:nil]; [e moveToTrash:nil];
    assert([e pending] && calls == 2 && scans == 5); [e release];
    e = [[FakeTrashExplorer alloc] init]; [e prepare:scan(dir)];
    workspaceAnswer = NO; workspaceTag = 7; [e moveToTrash:nil];
    assert(calls == 3 && [e pending] && scans == 6); [e release];
    e = [[FakeTrashExplorer alloc] init]; [e prepare:scan(dir)];
    workspaceAnswer = YES; workspaceTag = -1; [e moveToTrash:nil];
    assert(calls == 4 && ![e pending] && scans == 7);
    assert([[e message] rangeOfString:@"failed"].location != NSNotFound); [e release];
    e = [[FakeTrashExplorer alloc] init]; [e prepare:scan(dir)];
    workspaceAnswer = NO; workspaceTag = 0; [e moveToTrash:nil];
    assert(calls == 5 && ![e pending] && scans == 8);
    assert([[e message] rangeOfString:@"failed"].location != NSNotFound); [e release];
    e = [[FakeTrashExplorer alloc] init]; [e prepare:scan(dir)];
    replaceDuring = 1; [e moveToTrash:nil]; replaceDuring = 0;
    assert(calls == 5 && [[e message] rangeOfString:@"changed during confirmation"].location != NSNotFound);
    [e release]; assert(!unlink(fixtureFile) && !rename(fixtureOld, fixtureFile));
    e = [[FakeTrashExplorer alloc] init]; [e prepare:scan(dir)];
    closeDuring = 1; [e moveToTrash:nil]; assert(calls == 5); [e release];
    closeDuring = 0; workspaceAnswer = YES; workspaceTag = 0; tickWorkspace = 0; tickDuring = 0;
    e = [[FakeTrashExplorer alloc] init]; [e prepare:scan(dir)]; [e useRealRefresh];
    [e moveToTrash:nil]; assert(calls == 6 && [e busyState]);
    {
        NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:10];
        while ([e busyState] && [deadline timeIntervalSinceNow] > 0)
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    assert(![e busyState] && [e chartShowsTree]);
    assert([[e message] rangeOfString:@"completed"].location != NSNotFound);
    [e windowWillClose:nil]; [e release];
    e = [[FakeTrashExplorer alloc] init]; [e prepare:scan(dir)]; [e useRealRefresh];
    workspaceTag = 42; [e moveToTrash:nil]; assert(calls == 7 && [e busyState]);
    {
        NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:10];
        while ([e busyState] && [deadline timeIntervalSinceNow] > 0)
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    assert(![e busyState] && [e chartShowsTree] && [e treeBytes] == 0 && [e pending]);
    f = fopen(path, "a"); assert(f && fwrite("changed", 1, 7, f) == 7 && !fclose(f));
    [e refresh:nil]; assert([e busyState]);
    {
        NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:10];
        while ([e busyState] && [deadline timeIntervalSinceNow] > 0)
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    assert(![e busyState] && [e chartShowsTree] && [e treeBytes] == 7 && [e pending]);
    assert([[e message] rangeOfString:warning].location != NSNotFound);
    [e moveToTrash:nil]; assert(calls == 7);
    [e windowWillClose:nil]; [e release];
    assert(!unlink(path) && !rmdir(dir));
    [pool drain]; puts("PASS fake workspace trash workflow"); return 0;
}
