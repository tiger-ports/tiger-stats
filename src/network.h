#ifndef STATS_TIGER_NETWORK_H
#define STATS_TIGER_NETWORK_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>

#define ST_NETWORK_MAX_INTERFACES 128
#define ST_NETWORK_MAX_INTERVAL 10.0

typedef struct {
    char name[IFNAMSIZ];
    unsigned int index;
    uint64_t received, sent;
} STNetworkEntry;
typedef struct {
    STNetworkEntry entries[ST_NETWORK_MAX_INTERFACES];
    size_t count;
    double time;
} STNetworkSample;
typedef struct { STNetworkSample previous; int have_previous; } STNetworkState;
typedef struct { double received, sent; } STNetworkRate;

int st_network_update(STNetworkState *state, const STNetworkSample *sample, STNetworkRate *rate);
int st_read_network(STNetworkSample *sample);
int st_format_network(char *buffer, size_t size, const STNetworkRate *rate);
#endif
