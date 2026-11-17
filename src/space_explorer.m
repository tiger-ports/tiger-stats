#import "space_explorer.h"
#import "space_chart.h"
#import "space_size.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < 1050
typedef int STTableIndex;
typedef int STWorkspaceTag;
#else
typedef NSInteger STTableIndex;
typedef NSInteger STWorkspaceTag;
#endif

/* No display fallback is permitted for filesystem operations. */
NSString *STSpaceExactPath(const char *s)
{
    NSString *value;
    const char *roundtrip;
    if (!s || !(value = [NSString stringWithUTF8String:s])) return nil;
    roundtrip = [value fileSystemRepresentation];
    return roundtrip && !strcmp(s, roundtrip) ? value : nil;
}

static NSString *spaceName(const char *s)
{
    NSString *name = s ? [NSString stringWithUTF8String:s] : nil;
    if (!name && s) name = [NSString stringWithCString:s encoding:NSISOLatin1StringEncoding];
    return name ? name : @"(unrepresentable name)";
}
static NSColor *spaceColor(size_t i)
{
    if (i >= 64) return [NSColor grayColor];
    return [NSColor colorWithCalibratedHue:(float)((i * 37u) % 100u) / 100.0f
                                 saturation:0.65f brightness:0.82f alpha:1.0f];
}
static NSTextField *label(NSRect frame)
{
    NSTextField *field = [[[NSTextField alloc] initWithFrame:frame] autorelease];
    [field setEditable:NO]; [field setSelectable:NO]; [field setBordered:NO];
    [field setDrawsBackground:NO];
    return field;
}

@interface STSpaceChartView : NSView { const STSpaceNode *node; }
- (void)setNode:(const STSpaceNode *)value;
@end
@implementation STSpaceChartView
- (void)setNode:(const STSpaceNode *)value { node = value; [self setNeedsDisplay:YES]; }
- (void)drawRect:(NSRect)dirty
{
    NSRect bounds = [self bounds];
    NSPoint center = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
    float radius = (float)(MIN(bounds.size.width, bounds.size.height) * 0.44);
    size_t i, limit;
    double start = 0, end = 0, last = 0;
    (void)dirty;
    [[NSColor whiteColor] set]; NSRectFill(bounds);
    if (!node || !node->child_count) return;
    limit = node->child_count < 64 ? node->child_count : 64;
    for (i = 0; i < limit; ++i) {
        NSBezierPath *path;
        if (!st_space_chart_slice(node, i, &start, &end)) continue;
        path = [NSBezierPath bezierPath];
        [path moveToPoint:center];
        [path appendBezierPathWithArcWithCenter:center radius:radius startAngle:(float)start endAngle:(float)end clockwise:NO];
        [path closePath]; [spaceColor(i) set]; [path fill]; last = end;
    }
    if (last > 0 && last < 360.0 && node->child_count > 64) {
        NSBezierPath *path = [NSBezierPath bezierPath];
        [path moveToPoint:center];
        [path appendBezierPathWithArcWithCenter:center radius:radius startAngle:(float)last endAngle:360 clockwise:NO];
        [path closePath]; [[NSColor grayColor] set]; [path fill];
    }
    [[NSColor whiteColor] set];
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(center.x-radius*0.52, center.y-radius*0.52, radius*1.04, radius*1.04)] fill];
}
@end

/* The result owns the tree until the main thread transfers it. */
@interface STSpaceResult : NSObject { @public STSpaceNode *tree; int error; }
@end
@implementation STSpaceResult
- (void)dealloc { st_space_free(tree); [super dealloc]; }
@end

@interface STSpaceExplorer (Private)
- (void)buildWindow;
- (void)finished:(STSpaceResult *)result;
- (void)updateView;
- (void)up:(id)sender;
- (void)enter:(id)sender;
- (void)selectPath:(NSString *)path;
- (void)refresh:(id)sender;
- (void)refreshTick:(NSTimer *)timer;
- (void)startRefreshTimer;
- (void)stopRefreshTimer;
- (void)launchWorker:(NSString *)path;
- (void)startScan;
- (void)moveToTrash:(id)sender;
- (BOOL)confirmTrash:(NSString *)path;
- (NSWorkspace *)trashWorkspace;
- (BOOL)recycleParent:(NSString *)parent basename:(NSString *)name tag:(STWorkspaceTag *)tag;
@end

/* Timer and explorer lifecycle are confined to the main thread. The timer
   retains this proxy, not the explorer; dealloc clears it before invalidation. */
@interface STSpaceRefreshTarget : NSObject { STSpaceExplorer *explorer; }
- (id)initWithExplorer:(STSpaceExplorer *)value;
- (void)clear;
- (void)tick:(NSTimer *)timer;
@end
@implementation STSpaceRefreshTarget
- (id)initWithExplorer:(STSpaceExplorer *)value
{
    self = [super init];
    if (self) explorer = value;
    return self;
}
- (void)clear { explorer = nil; }
- (void)tick:(NSTimer *)timer
{
    STSpaceExplorer *target = [explorer retain];
    [target refreshTick:timer];
    [target release];
}
@end

/* The thread retains this job, not its controller. startScan's separate
   retain keeps the controller alive until its main-thread completion. */
@interface STSpaceScanJob : NSObject { STSpaceExplorer *explorer; }
- (id)initWithExplorer:(STSpaceExplorer *)value;
- (void)run:(NSString *)path;
@end
@implementation STSpaceScanJob
- (id)initWithExplorer:(STSpaceExplorer *)value
{
    self = [super init];
    if (self) explorer = value;
    return self;
}
- (void)run:(NSString *)path
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    STSpaceScanOptions options = {10000, 64, 1};
    STSpaceResult *result = [[STSpaceResult alloc] init];
    const char *filename = [path fileSystemRepresentation];
    if (!filename) result->error = EINVAL;
    else if (st_space_scan(filename, &options, &result->tree) != 0) result->error = errno ? errno : EIO;
    [explorer performSelectorOnMainThread:@selector(finished:) withObject:result waitUntilDone:NO];
    /* Do not touch explorer after scheduling: completion can release it. */
    [result release]; [pool drain];
}
@end

@implementation STSpaceExplorer
- (void)buildWindow
{
    NSView *content;
    NSScrollView *scroll;
    NSTableColumn *column;
    if (window) return;
    window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 820, 520)
        styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask)
        backing:NSBackingStoreBuffered defer:NO];
    [window setReleasedWhenClosed:NO]; [window setDelegate:(id)self];
    [window setTitle:@"Disk Space Explorer"];
    content = [window contentView];
    pathLabel = [label(NSMakeRect(20, 482, 780, 20)) retain]; [content addSubview:pathLabel];
    status = [label(NSMakeRect(20, 428, 780, 48)) retain];
    [[status cell] setWraps:YES]; [content addSubview:status];
    chart = [[STSpaceChartView alloc] initWithFrame:NSMakeRect(20, 135, 300, 280)]; [content addSubview:chart];
    scroll = [[[NSScrollView alloc] initWithFrame:NSMakeRect(330, 100, 470, 315)] autorelease];
    [scroll setHasVerticalScroller:YES]; [scroll setBorderType:NSBezelBorder];
    table = [[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, 450, 315)];
    column = [[[NSTableColumn alloc] initWithIdentifier:@"name"] autorelease];
    [[column headerCell] setStringValue:@"Item (chart colour)"]; [column setWidth:310]; [table addTableColumn:column];
    column = [[[NSTableColumn alloc] initWithIdentifier:@"size"] autorelease];
    [[column headerCell] setStringValue:@"Logical size"]; [column setWidth:140]; [table addTableColumn:column];
    [table setDataSource:(id)self]; [table setDelegate:(id)self];
    [table setAllowsMultipleSelection:NO];
    [table setTarget:self]; [table setDoubleAction:@selector(enter:)];
    [scroll setDocumentView:table]; [content addSubview:scroll];
    upButton = [[NSButton alloc] initWithFrame:NSMakeRect(330, 52, 55, 30)];
    [upButton setTitle:@"Up"]; [upButton setTarget:self]; [upButton setAction:@selector(up:)]; [content addSubview:upButton];
    openButton = [[NSButton alloc] initWithFrame:NSMakeRect(390, 52, 155, 30)];
    [openButton setTitle:@"Open selected folder"]; [openButton setTarget:self]; [openButton setAction:@selector(enter:)]; [content addSubview:openButton];
    refreshButton = [[NSButton alloc] initWithFrame:NSMakeRect(550, 52, 90, 30)];
    [refreshButton setTitle:@"Refresh"]; [refreshButton setTarget:self]; [refreshButton setAction:@selector(refresh:)]; [content addSubview:refreshButton];
    trashButton = [[NSButton alloc] initWithFrame:NSMakeRect(645, 52, 155, 30)];
    [trashButton setTitle:@"Move to Trash"]; [trashButton setTarget:self]; [trashButton setAction:@selector(moveToTrash:)]; [trashButton setEnabled:NO]; [content addSubview:trashButton];
    [content addSubview:label(NSMakeRect(20, 20, 780, 25))];
    [[[content subviews] lastObject] setStringValue:@"First 64 items use chart colours; remaining items are grey. Double-click a folder to open it."];
}
- (void)explore:(id)sender
{
    NSOpenPanel *panel;
    NSString *path = nil;
    NSString *directory;
    (void)sender;
    if (choosing || trashConfirming) return;
    if (trashPending) {
        if (closed) {
            closed = NO; pendingScan = YES;
            [self startRefreshTimer];
            [status setStringValue:operationMessage];
            [self startScan];
        }
        [window makeKeyAndOrderFront:self];
        return;
    }
    if (scanning) {
        if (closed && selectedPath) [self selectPath:selectedPath];
        [window makeKeyAndOrderFront:self]; return;
    }
    choosing = YES;
    panel = [NSOpenPanel openPanel];
    if (!panel) {
        choosing = NO;
        return;
    }
    [panel setCanChooseFiles:NO]; [panel setCanChooseDirectories:YES]; [panel setAllowsMultipleSelection:NO];
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < 1050
    /* Tiger 10.4 requires explicit directory; nil causes uninitialized state/crash */
    directory = NSHomeDirectory();
#else
    directory = nil;
#endif
    if ([panel runModalForDirectory:directory file:nil types:nil] == NSOKButton && [[panel filenames] count])
        path = [[[panel filenames] objectAtIndex:0] retain];
    choosing = NO;
    if (path) { [self selectPath:path]; [path release]; }
    else if (pendingScan && !closed) [self startScan];
}
- (void)selectPath:(NSString *)path
{
    NSString *saved = [path copy];
    [self buildWindow];
    [selectedPath release]; selectedPath = saved;
    [operationMessage release]; operationMessage = nil;
    ++generation; closed = NO; pendingScan = YES; scanError = 0;
    [(STSpaceChartView *)chart setNode:NULL];
    st_space_nav_init(&navigation, NULL); st_space_free(root); root = NULL;
    [pathLabel setStringValue:selectedPath]; [table reloadData];
    [upButton setEnabled:NO]; [openButton setEnabled:NO]; [trashButton setEnabled:NO];
    [status setStringValue:@"Waiting to scan in background."];
    [window makeKeyAndOrderFront:self];
    [self startRefreshTimer];
    [self startScan];
}
- (void)startRefreshTimer
{
    NSRunLoop *loop = [NSRunLoop currentRunLoop];
    [self stopRefreshTimer];
    refreshTarget = [[STSpaceRefreshTarget alloc] initWithExplorer:self];
    refreshTimer = [[NSTimer timerWithTimeInterval:30.0 target:refreshTarget selector:@selector(tick:) userInfo:nil repeats:YES] retain];
    [loop addTimer:refreshTimer forMode:NSDefaultRunLoopMode];
    [loop addTimer:refreshTimer forMode:NSEventTrackingRunLoopMode];
}
- (void)stopRefreshTimer
{
    [refreshTarget clear];
    [refreshTimer invalidate]; [refreshTimer release]; refreshTimer = nil;
    [refreshTarget release]; refreshTarget = nil;
}
- (void)refreshTick:(NSTimer *)timer { (void)timer; [self refresh:nil]; }
- (void)refresh:(id)sender
{
    (void)sender;
    if (closed || choosing || trashConfirming || scanning || !selectedPath) return;
    pendingScan = YES; [self startScan];
}
- (void)startScan
{
    if (closed || choosing || trashConfirming || scanning || !pendingScan || !selectedPath) return;
    pendingScan = NO; scanning = YES; scanGeneration = generation;
    [refreshButton setEnabled:NO];
    [trashButton setEnabled:NO];
    if (root) [self updateView];
    else [status setStringValue:[NSString stringWithFormat:@"%@Scanning in background. Closing this window does not cancel the scan.", operationMessage ? operationMessage : @""]];
    [self retain];
    [self launchWorker:selectedPath];
}
- (void)launchWorker:(NSString *)path
{
    STSpaceScanJob *job = [[STSpaceScanJob alloc] initWithExplorer:self];
    [NSThread detachNewThreadSelector:@selector(run:) toTarget:job withObject:path];
    [job release];
}
- (void)finished:(STSpaceResult *)result
{
    scanning = NO;
    if (!closed && scanGeneration == generation) {
        scanError = result->error;
        if (result->error) {
            if (root) [self updateView];
            else [status setStringValue:[NSString stringWithFormat:@"%@Scan failed: %@", operationMessage ? operationMessage : @"", spaceName(strerror(result->error))]];
        } else {
            STSpaceNode *old = root;
            [(STSpaceChartView *)chart setNode:NULL];
            st_space_nav_init(&navigation, NULL);
            root = result->tree; result->tree = NULL;
            st_space_free(old);
            st_space_nav_init(&navigation, root); [self updateView];
        }
        [refreshButton setEnabled:YES];
    }
    if (pendingScan && !closed) [self startScan];
    [self release];
}
- (void)updateView
{
    const STSpaceNode *node = st_space_nav_current(&navigation);
    NSMutableString *path;
    size_t i;
    if (!node) return;
    path = [NSMutableString stringWithString:spaceName(navigation.stack[0]->name)];
    for (i = 1; i <= navigation.depth; ++i) {
        if (![path hasSuffix:@"/"]) [path appendString:@"/"];
        [path appendString:spaceName(navigation.stack[i]->name)];
    }
    [pathLabel setStringValue:path];
    {
        char sizebuf[32];
        if (!st_space_format_size(node->bytes, sizebuf, sizeof sizebuf))
            snprintf(sizebuf, sizeof sizebuf, "%llu B", (unsigned long long)node->bytes);
        [status setStringValue:[NSString stringWithFormat:@"%@%@%@%@%@Logical size: %s — not allocated or reclaimable space. Hard links counted once per scan; symlinks are not followed.",
            operationMessage ? operationMessage : @"",
            scanError ? [NSString stringWithFormat:@"Refresh failed: %@; showing previous results. ", spaceName(strerror(scanError))] : @"",
            (root->flags & ST_SPACE_INCOMPLETE) ? @"SCAN INCOMPLETE (limits, errors, or volume boundaries). " : @"",
            (node->flags & ST_SPACE_UNREADABLE) ? @"THIS FOLDER UNREADABLE. " : @"",
            scanning ? @"Refreshing in background. " : @"",
            sizebuf]];
    }
    [(STSpaceChartView *)chart setNode:node]; [table reloadData];
    [upButton setEnabled:navigation.depth > 0]; [openButton setEnabled:NO]; [trashButton setEnabled:NO];
}
- (STTableIndex)numberOfRowsInTableView:(NSTableView *)view
{
    const STSpaceNode *node = st_space_nav_current(&navigation); (void)view;
    return node ? (STTableIndex)node->child_count : 0;
}
- (id)tableView:(NSTableView *)view objectValueForTableColumn:(NSTableColumn *)column row:(STTableIndex)row
{
    const STSpaceNode *node = st_space_nav_current(&navigation), *child;
    NSString *suffix = @"";
    (void)view;
    if (!node || row < 0 || (size_t)row >= node->child_count) return @"";
    child = node->children[row];
    if ([[column identifier] isEqualToString:@"size"]) {
        char buf[32];
        if (st_space_format_size(child->bytes, buf, sizeof buf)) return [NSString stringWithUTF8String:buf];
        return [NSString stringWithFormat:@"%llu B", (unsigned long long)child->bytes];
    }
    if (child->flags & ST_SPACE_SYMLINK) suffix = @" [symlink, excluded]";
    else if (child->flags & ST_SPACE_UNREADABLE) suffix = @" [unreadable]";
    else if (child->flags & ST_SPACE_INCOMPLETE) suffix = @" [incomplete]";
    else if (child->flags & ST_SPACE_DIRECTORY) suffix = @" /";
    return [NSString stringWithFormat:@"%@%@", spaceName(child->name), suffix];
}
- (void)tableView:(NSTableView *)view willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)column row:(STTableIndex)row
{
    (void)view;
    if ([[column identifier] isEqualToString:@"name"] && [cell respondsToSelector:@selector(setTextColor:)])
        [cell setTextColor:spaceColor((size_t)row)];
}
- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    const STSpaceNode *node = st_space_nav_current(&navigation);
    STTableIndex row = (STTableIndex)[table selectedRow]; (void)note;
    [openButton setEnabled:node && row >= 0 && (size_t)row < node->child_count &&
        (node->children[row]->flags & ST_SPACE_DIRECTORY) != 0];
    [trashButton setEnabled:!closed && !scanning && !choosing && !trashConfirming && !trashPending &&
        node && row >= 0 && (size_t)row < node->child_count &&
        node->children[row]->identity.valid && S_ISREG(node->children[row]->identity.mode) &&
        node->children[row]->identity.dev == root->identity.dev];
}
- (void)up:(id)sender { (void)sender; if (st_space_nav_up(&navigation)) [self updateView]; }
- (void)enter:(id)sender
{
    STTableIndex row = (STTableIndex)[table selectedRow]; (void)sender;
    if (row >= 0 && st_space_nav_enter(&navigation, (size_t)row)) [self updateView];
}
- (BOOL)confirmTrash:(NSString *)path
{
    return NSRunAlertPanel(@"Move this file to Trash?",
        @"%@\n\nTrash retains the file until emptied. Logical bytes are not necessarily reclaimable space.",
        @"Move to Trash", @"Cancel", nil, path) == NSAlertDefaultReturn;
}
- (BOOL)recycleParent:(NSString *)parent basename:(NSString *)name tag:(STWorkspaceTag *)tag
{
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < 1060
    /* The Tiger-era reference specifies an empty string. */
    NSString *destination = @"";
#else
    /* Modern AppKit headers specify nil for operations without a destination. */
    NSString *destination = nil;
#endif
    return [[self trashWorkspace] performFileOperation:NSWorkspaceRecycleOperation
        source:parent destination:destination files:[NSArray arrayWithObject:name] tag:tag];
}
- (NSWorkspace *)trashWorkspace { return [NSWorkspace sharedWorkspace]; }
- (void)moveToTrash:(id)sender
{
    const STSpaceNode *node, *file;
    STTableIndex row;
    NSMutableData *bytes;
    NSString *parent, *basename, *full;
    size_t i;
    char zero = 0;
    unsigned long before;
    BOOL accepted, result;
    STWorkspaceTag tag = 0;
    (void)sender;
    if (closed || choosing || scanning || trashConfirming || trashPending || !root ||
        [table numberOfSelectedRows] != 1) return;
    node = st_space_nav_current(&navigation); row = (STTableIndex)[table selectedRow];
    if (!node || row < 0 || (size_t)row >= node->child_count) return;
    file = node->children[row];
    if (!st_space_trash_validate(root, navigation.stack, navigation.depth + 1, file)) {
        [status setStringValue:@"Cannot move this item: its type or scanned identity is unavailable or has changed. Refresh first."];
        return;
    }
    bytes = [NSMutableData data];
    [bytes appendBytes:root->name length:strlen(root->name)];
    for (i = 1; i <= navigation.depth; ++i) {
        const char *name = navigation.stack[i]->name;
        if (((const char *)[bytes bytes])[[bytes length]-1] != '/') [bytes appendBytes:"/" length:1];
        [bytes appendBytes:name length:strlen(name)];
    }
    [bytes appendBytes:&zero length:1];
    parent = STSpaceExactPath((const char *)[bytes bytes]);
    basename = STSpaceExactPath(file->name);
    [bytes setLength:[bytes length]-1];
    if (((const char *)[bytes bytes])[[bytes length]-1] != '/') [bytes appendBytes:"/" length:1];
    [bytes appendBytes:file->name length:strlen(file->name)]; [bytes appendBytes:&zero length:1];
    full = STSpaceExactPath((const char *)[bytes bytes]);
    if (!parent || !basename || !full) {
        [status setStringValue:@"Cannot safely represent this exact filesystem name. Nothing was moved."];
        return;
    }
    /* Modal event loops may close the window; retain self and never dereference
       the old tree after a close/generation change. */
    before = generation; trashConfirming = YES; [trashButton setEnabled:NO];
    [self retain];
    accepted = [self confirmTrash:full];
    trashConfirming = NO;
    if (!accepted || closed || before != generation || scanning || trashPending) {
        if (!closed) [self tableViewSelectionDidChange:nil];
        [self release]; return;
    }
    if (!st_space_trash_validate(root, navigation.stack, navigation.depth + 1, file)) {
        [status setStringValue:@"Item or ancestor changed during confirmation. Nothing was moved; refresh first."];
        [self tableViewSelectionDidChange:nil]; [self release]; return;
    }
    trashConfirming = YES; /* also suppress scans in a nested workspace event loop */
    result = [self recycleParent:parent basename:basename tag:&tag];
    trashConfirming = NO;
    /* A positive tag may mean work remains in flight, even if the return value
       is inconsistent. Never infer completion from a notification. */
    ++generation; [(STSpaceChartView *)chart setNode:NULL];
    st_space_nav_init(&navigation, NULL); st_space_free(root); root = NULL;
    [table reloadData]; [trashButton setEnabled:NO]; [openButton setEnabled:NO]; [upButton setEnabled:NO];
    if (tag > 0) {
        trashPending = YES; pendingScan = YES;
        [operationMessage release];
        operationMessage = [@"Trash operation may still be pending; completion unverified. Chart is provisional; further Trash disabled. " copy];
        [status setStringValue:operationMessage];
        [self startScan];
    } else {
        pendingScan = YES;
        [operationMessage release];
        operationMessage = [(!result || tag < 0 ? @"Trash operation failed or returned an invalid tag. " :
            @"Move to Trash completed; Trash still occupies space until emptied. ") copy];
        [status setStringValue:operationMessage];
        [self startScan];
    }
    [self release];
}
- (void)windowWillClose:(NSNotification *)note
{
    (void)note; closed = YES; ++generation; pendingScan = NO;
    [self stopRefreshTimer];
    [(STSpaceChartView *)chart setNode:NULL];
    st_space_nav_init(&navigation, NULL); st_space_free(root); root = NULL;
}
- (void)dealloc
{
    [window setDelegate:nil]; [table setDataSource:nil]; [table setDelegate:nil];
    [self stopRefreshTimer]; [selectedPath release]; [operationMessage release];
    st_space_free(root); [pathLabel release]; [status release]; [table release]; [chart release];
    [upButton release]; [openButton release]; [refreshButton release]; [trashButton release]; [window release]; [super dealloc];
}
@end
