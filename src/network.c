#include "network.h"
#include <string.h>
#include <float.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <sys/time.h>

static int valid_sample(const STNetworkSample *sample);

int st_read_network(STNetworkSample *sample)
{
    struct ifaddrs *list, *p;
    struct timeval now;
    STNetworkSample current;
    int ok = 1;
    if (!sample || gettimeofday(&now, NULL) != 0 ||
        now.tv_sec <= 0 || now.tv_usec < 0 || now.tv_usec >= 1000000 ||
        getifaddrs(&list) != 0) return -1;
    memset(&current, 0, sizeof(current));
    current.time = (double)now.tv_sec + (double)now.tv_usec / 1000000.0;
    for (p = list; p; p = p->ifa_next) {
        const struct sockaddr_dl *link;
        const struct if_data *data;
        STNetworkEntry *entry;
        size_t len;
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_LINK ||
            !(p->ifa_flags & IFF_UP) || !(p->ifa_flags & IFF_RUNNING) ||
            (p->ifa_flags & IFF_LOOPBACK)) continue;
        link = (const struct sockaddr_dl *)p->ifa_addr;
        if (!p->ifa_name || !p->ifa_data ||
            link->sdl_len < offsetof(struct sockaddr_dl, sdl_index) + sizeof(link->sdl_index) ||
            current.count == ST_NETWORK_MAX_INTERFACES) { ok = 0; break; }
        for (len = 0; len < IFNAMSIZ && p->ifa_name[len]; len++) {}
        if (!len || len == IFNAMSIZ || !link->sdl_index) { ok = 0; break; }
        entry = &current.entries[current.count++];
        memcpy(entry->name, p->ifa_name, len + 1);
        entry->index = link->sdl_index;
        data = (const struct if_data *)p->ifa_data;
        entry->received = (uint64_t)data->ifi_ibytes;
        entry->sent = (uint64_t)data->ifi_obytes;
    }
    if (list) freeifaddrs(list);
    if (!ok || !valid_sample(&current)) return -1;
    *sample = current;
    return 0;
}

static int valid_sample(const STNetworkSample *sample)
{
    size_t i, j;
    if (!sample || !sample->count || sample->count > ST_NETWORK_MAX_INTERFACES ||
        !(sample->time > 0.0 && sample->time <= DBL_MAX)) return 0;
    for (i = 0; i < sample->count; i++) {
        if (!sample->entries[i].index || !sample->entries[i].name[0] ||
            !memchr(sample->entries[i].name, '\0', sizeof(sample->entries[i].name))) return 0;
        for (j = 0; j < i; j++)
            if (sample->entries[i].index == sample->entries[j].index ||
                strcmp(sample->entries[i].name, sample->entries[j].name) == 0) return 0;
    }
    return 1;
}

int st_format_network(char *buffer, size_t size, const STNetworkRate *rate)
{
    char text[128];
    int n;
    if (!buffer || !rate || !(rate->received >= 0 && rate->received <= DBL_MAX) ||
        !(rate->sent >= 0 && rate->sent <= DBL_MAX)) return -1;
    n = snprintf(text, sizeof(text), "Network: down %.1f / up %.1f KiB/s",
                 rate->received / 1024.0, rate->sent / 1024.0);
    if (n < 0 || (size_t)n >= sizeof(text) || (size_t)n >= size) return -1;
    memcpy(buffer, text, (size_t)n + 1);
    return 0;
}

int st_network_update(STNetworkState *state, const STNetworkSample *sample, STNetworkRate *rate)
{
    size_t i, j;
    double received = 0, sent = 0, elapsed;
    int ready;
    if (!state) return -1;
    if (!rate || !valid_sample(sample)) { state->have_previous = 0; return -1; }
    ready = state->have_previous && sample->count == state->previous.count;
    elapsed = sample->time - state->previous.time;
    if (!(elapsed > 0.0 && elapsed <= ST_NETWORK_MAX_INTERVAL)) ready = 0;
    if (ready) {
        for (i = 0; i < sample->count; i++) {
            for (j = 0; j < state->previous.count; j++)
                if (sample->entries[i].index == state->previous.entries[j].index &&
                    strcmp(sample->entries[i].name, state->previous.entries[j].name) == 0) break;
            if (j == state->previous.count ||
                sample->entries[i].received < state->previous.entries[j].received ||
                sample->entries[i].sent < state->previous.entries[j].sent) {
                ready = 0;
                break;
            }
            received += (double)(sample->entries[i].received - state->previous.entries[j].received);
            sent += (double)(sample->entries[i].sent - state->previous.entries[j].sent);
        }
        if (ready) {
            double received_rate = received / elapsed;
            double sent_rate = sent / elapsed;
            if (!(received_rate <= DBL_MAX && sent_rate <= DBL_MAX)) ready = 0;
            else { rate->received = received_rate; rate->sent = sent_rate; }
        }
    }
    state->previous = *sample;
    state->have_previous = 1;
    return ready && elapsed > 0 ? 0 : -1;
}
