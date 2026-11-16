#import <Cocoa/Cocoa.h>
#import "space_navigation.h"

@interface STSpaceExplorer : NSObject {
    NSWindow *window;
    NSTextField *status;
    NSTextField *pathLabel;
    NSTableView *table;
    NSView *chart;
    NSButton *upButton;
    NSButton *openButton;
    NSButton *refreshButton;
    NSButton *trashButton;
    NSTimer *refreshTimer;
    id refreshTarget;
    NSString *selectedPath;
    NSString *operationMessage;
    unsigned long generation;
    unsigned long scanGeneration;
    int scanError;
    BOOL choosing;
    BOOL trashConfirming;
    BOOL trashPending;
    BOOL pendingScan;
    STSpaceNode *root;
    STSpaceNavigation navigation;
    BOOL scanning;
    BOOL closed;
}
- (void)explore:(id)sender;
@end
