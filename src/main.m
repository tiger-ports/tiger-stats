#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "monitor.h"
#include "network.h"
#import "timer_support.h"
#import "space_explorer.h"

@interface STApp : NSObject {
    NSStatusItem *statusItem;
    NSMenu *menu;
    NSMenuItem *cpuItem;
    NSMenuItem *memoryItem;
    NSMenuItem *diskItem;
    NSMenuItem *networkItem;
    STNetworkState networkState;
    NSTimer *timer;
    STSpaceExplorer *explorer;
    STCPUTicks previous;
    int havePrevious;
}
- (void)refresh:(NSTimer *)unused;
- (void)launchActivityMonitor:(id)sender;
- (void)quit:(id)sender;
@end

@implementation STApp
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    NSMenuItem *quitItem;
    NSMenuItem *exploreItem;
    (void)notification;
    statusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength] retain];
    [statusItem setTitle:@"CPU: sampling"];
    [statusItem setHighlightMode:YES];
    menu = [[NSMenu alloc] initWithTitle:@"Stats Tiger"];
    [menu setAutoenablesItems:NO];
    cpuItem = [[NSMenuItem alloc] initWithTitle:@"CPU: sampling" action:NULL keyEquivalent:@""];
    memoryItem = [[NSMenuItem alloc] initWithTitle:@"Memory: unavailable" action:NULL keyEquivalent:@""];
    diskItem = [[NSMenuItem alloc] initWithTitle:@"Root disk available: unavailable" action:NULL keyEquivalent:@""];
    networkItem = [[NSMenuItem alloc] initWithTitle:@"Network: unavailable" action:NULL keyEquivalent:@""];
    [networkItem setEnabled:NO];
    [cpuItem setEnabled:NO];
    [memoryItem setEnabled:NO];
    [diskItem setEnabled:NO];
    [menu addItem:cpuItem];
    [menu addItem:memoryItem];
    [menu addItem:diskItem];
    [menu addItem:networkItem];
    [menu addItem:[NSMenuItem separatorItem]];
    explorer = [[STSpaceExplorer alloc] init];
    exploreItem = [[NSMenuItem alloc] initWithTitle:@"Explore Disk Space..." action:@selector(explore:) keyEquivalent:@""];
    [exploreItem setTarget:explorer];
    [menu addItem:exploreItem];
    [exploreItem release];
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *activityMonitorItem = [[NSMenuItem alloc] initWithTitle:@"Open Activity Monitor" action:@selector(launchActivityMonitor:) keyEquivalent:@""];
    [activityMonitorItem setTarget:self];
    [menu addItem:activityMonitorItem];
    [activityMonitorItem release];
    [menu addItem:[NSMenuItem separatorItem]];
    quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit Stats Tiger" action:@selector(quit:) keyEquivalent:@"q"];
    [quitItem setTarget:self];
    [menu addItem:quitItem];
    [quitItem release];
    [statusItem setMenu:menu];
    [self refresh:nil];
    timer = [[NSTimer timerWithTimeInterval:1.0 target:self selector:@selector(refresh:) userInfo:nil repeats:YES] retain];
    st_schedule_update_timer(timer);
}

- (void)refresh:(NSTimer *)unused
{
    STCPUTicks current;
    STMemory memory;
    STDisk disk;
    STNetworkSample network;
    STNetworkRate networkRate;
    double percent;
    char text[128];
    NSString *title;
    (void)unused;
    if (st_read_cpu(&current) == 0) {
        if (havePrevious && st_cpu_percent(&previous, &current, &percent) == 0 &&
            st_format_cpu(text, sizeof(text), percent) == 0) {
            title = [NSString stringWithUTF8String:text];
            [cpuItem setTitle:title];
            [statusItem setTitle:title];
        } else if (havePrevious) {
            [cpuItem setTitle:@"CPU: unavailable"];
            [statusItem setTitle:@"CPU: --"];
        }
        previous = current;
        havePrevious = 1;
    } else {
        havePrevious = 0;
        [cpuItem setTitle:@"CPU: unavailable"];
        [statusItem setTitle:@"CPU: --"];
    }
    if (st_read_memory(&memory) == 0 && st_format_memory(text, sizeof(text), &memory) == 0)
        [memoryItem setTitle:[NSString stringWithUTF8String:text]];
    else
        [memoryItem setTitle:@"Memory: unavailable"];
    if (st_read_disk(&disk) == 0 && st_format_disk(text, sizeof(text), &disk) == 0)
        [diskItem setTitle:[NSString stringWithUTF8String:text]];
    else
        [diskItem setTitle:@"Root disk available: unavailable"];
    if (st_read_network(&network) != 0) {
        st_network_update(&networkState, NULL, &networkRate);
        [networkItem setTitle:@"Network: unavailable"];
    } else if (st_network_update(&networkState, &network, &networkRate) == 0 &&
               st_format_network(text, sizeof(text), &networkRate) == 0) {
        [networkItem setTitle:[NSString stringWithUTF8String:text]];
    } else {
        [networkItem setTitle:@"Network: unavailable"];
    }
}

- (void)launchActivityMonitor:(id)sender
{
    NSTask *task;
    NSArray *arguments;
    (void)sender;
    task = [[[NSTask alloc] init] autorelease];
    [task setLaunchPath:@"/usr/bin/open"];
    arguments = [NSArray arrayWithObjects:@"-a", @"Activity Monitor", nil];
    [task setArguments:arguments];
    [task launch];
}

- (void)quit:(id)sender
{
    (void)sender;
    [NSApp terminate:self];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    (void)notification;
    [timer invalidate];
    [timer release]; timer = nil;
    [[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
    [statusItem release]; statusItem = nil;
    [cpuItem release]; cpuItem = nil;
    [memoryItem release]; memoryItem = nil;
    [diskItem release]; diskItem = nil;
    [networkItem release]; networkItem = nil;
    [menu release]; menu = nil;
    [explorer release]; explorer = nil;
}
@end

static int self_check(void)
{
    STCPUTicks first, second;
    STMemory memory;
    STDisk disk;
    STNetworkSample network;
    STNetworkRate networkRate;
    STNetworkState networkState;
    double percent;
    char cpu[128], mem[128], disk_text[128], network_text[128];
    memset(&networkState, 0, sizeof(networkState));
    if (st_read_cpu(&first) != 0) { fprintf(stderr, "CPU sample failed\n"); return 1; }
    if (st_read_network(&network) != 0) { fprintf(stderr, "Network sample failed\n"); return 1; }
    st_network_update(&networkState, &network, &networkRate);
    sleep(1);
    if (st_read_network(&network) != 0 ||
        st_network_update(&networkState, &network, &networkRate) != 0 ||
        st_format_network(network_text, sizeof(network_text), &networkRate) != 0) {
        fprintf(stderr, "Network delta failed\n"); return 1;
    }
    if (st_read_cpu(&second) != 0 || st_cpu_percent(&first, &second, &percent) != 0 ||
        st_format_cpu(cpu, sizeof(cpu), percent) != 0) {
        fprintf(stderr, "CPU delta failed\n"); return 1;
    }
    if (st_read_memory(&memory) != 0 || st_format_memory(mem, sizeof(mem), &memory) != 0) {
        fprintf(stderr, "Memory sample failed\n"); return 1;
    }
    if (st_read_disk(&disk) != 0 || st_format_disk(disk_text, sizeof(disk_text), &disk) != 0) {
        fprintf(stderr, "Root disk sample failed\n"); return 1;
    }
    printf("%s\n%s\n%s\n%s\n", cpu, mem, disk_text, network_text);
    return 0;
}

int main(int argc, char **argv)
{
    NSAutoreleasePool *pool;
    NSApplication *app;
    STApp *delegate;
    if (argc == 2 && strcmp(argv[1], "--self-check") == 0) return self_check();
    if (argc > 1 && !(argc == 2 && strncmp(argv[1], "-psn_", 5) == 0)) {
        fprintf(stderr, "Usage: %s [--self-check]\n", argv[0]); return 2;
    }
    pool = [[NSAutoreleasePool alloc] init];
    app = [NSApplication sharedApplication];
    delegate = [[STApp alloc] init];
    [app setDelegate:(id)delegate];
    [app run];
    [app setDelegate:nil];
    [delegate release];
    [pool drain];
    return 0;
}
