#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include "../src/network.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static struct ifaddrs nodes[5];
static struct sockaddr_dl links[5];
static struct if_data data[5];
static int fail_read, fail_clock, freed;
static int mock_getifaddrs(struct ifaddrs **out);
static void mock_freeifaddrs(struct ifaddrs *list);
static int mock_gettimeofday(struct timeval *tv, void *zone);
#define getifaddrs mock_getifaddrs
#define freeifaddrs mock_freeifaddrs
#define gettimeofday mock_gettimeofday
#include "../src/network.c"
#undef getifaddrs
#undef freeifaddrs
#undef gettimeofday
static int mock_getifaddrs(struct ifaddrs **out) { if (fail_read) return -1; *out = nodes; return 0; }
static void mock_freeifaddrs(struct ifaddrs *list) { if (list == nodes) freed++; }
static int mock_gettimeofday(struct timeval *tv, void *zone) { (void)zone; if (fail_clock) return -1; tv->tv_sec = 10; tv->tv_usec = 250000; return 0; }
int main(void)
{
    STNetworkSample s, original;
    int i;
    const char *names[] = {"en0", "lo0", "en1", "en2", "en0"};
    memset(nodes, 0, sizeof(nodes)); memset(links, 0, sizeof(links)); memset(data, 0, sizeof(data));
    for (i = 0; i < 5; i++) {
        nodes[i].ifa_next = i == 4 ? NULL : &nodes[i+1];
        nodes[i].ifa_name = (char *)names[i];
        nodes[i].ifa_flags = IFF_UP | IFF_RUNNING;
        nodes[i].ifa_addr = (struct sockaddr *)&links[i]; nodes[i].ifa_data = &data[i];
        links[i].sdl_len = sizeof(links[i]); links[i].sdl_family = AF_LINK; links[i].sdl_index = (unsigned short)(i+1);
    }
    nodes[1].ifa_flags |= IFF_LOOPBACK;
    nodes[2].ifa_flags &= ~IFF_UP;
    nodes[3].ifa_flags &= ~IFF_RUNNING;
    links[4].sdl_family = AF_INET;
    data[0].ifi_ibytes = 100; data[0].ifi_obytes = 200;
    CHECK(st_read_network(&s) == 0 && freed == 1 && s.count == 1);
    CHECK(strcmp(s.entries[0].name, "en0") == 0 && s.entries[0].index == 1);
    CHECK(s.entries[0].received == 100 && s.entries[0].sent == 200 && s.time == 10.25);
    original = s; fail_read = 1;
    CHECK(st_read_network(&s) != 0 && !memcmp(&s, &original, sizeof(s)) && freed == 1);
    fail_read = 0; fail_clock = 1;
    CHECK(st_read_network(&s) != 0 && !memcmp(&s, &original, sizeof(s)));
    fail_clock = 0; nodes[0].ifa_data = NULL;
    CHECK(st_read_network(&s) != 0 && !memcmp(&s, &original, sizeof(s)));
    nodes[0].ifa_data = &data[0];
    links[0].sdl_len = 2;
    CHECK(st_read_network(&s) != 0 && !memcmp(&s, &original, sizeof(s)));
    links[0].sdl_len = sizeof(links[0]);
    nodes[2].ifa_flags = IFF_UP | IFF_RUNNING;
    nodes[2].ifa_name = nodes[0].ifa_name;
    CHECK(st_read_network(&s) != 0 && !memcmp(&s, &original, sizeof(s)));
    nodes[2].ifa_flags = 0; nodes[0].ifa_flags = 0;
    CHECK(st_read_network(&s) != 0 && !memcmp(&s, &original, sizeof(s)));
    CHECK(st_read_network(NULL) != 0);
    puts("PASS network reader filtering and failure");
    return 0;
}
