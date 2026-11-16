#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include "../src/network.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void)
{
    STNetworkState state = {0};
    STNetworkSample a = {0}, b;
    STNetworkRate rate = {0};
    a.count = 1;
    strcpy(a.entries[0].name, "en0");
    a.entries[0].index = 2;
    a.entries[0].received = 100;
    a.entries[0].sent = 200;
    a.time = 10;
    b = a;
    b.time = 12;
    b.entries[0].received = 300;
    b.entries[0].sent = 260;
    CHECK(st_network_update(&state, &a, &rate) != 0);
    CHECK(st_network_update(&state, &b, &rate) == 0);
    CHECK(fabs(rate.received - 100.0) < 0.0001);
    CHECK(fabs(rate.sent - 30.0) < 0.0001);
    puts("PASS network throughput");
    /* A decrease must not be interpreted as counter wrap. */
    a = b; a.time = 13; a.entries[0].received = 20;
    rate.received = 987; rate.sent = 654;
    CHECK(st_network_update(&state, &a, &rate) != 0);
    CHECK(rate.received == 987 && rate.sent == 654);
    b = a; b.time = 14; b.entries[0].received += 10; b.entries[0].sent += 5;
    CHECK(st_network_update(&state, &b, &rate) == 0 && rate.received == 10 && rate.sent == 5);
    puts("PASS counter decrease and rebaseline");
    a = b; a.time++; strcpy(a.entries[0].name, "en1"); a.entries[0].index = 3;
    CHECK(st_network_update(&state, &a, &rate) != 0);
    b = a; b.time++; b.entries[0].received += 4;
    CHECK(st_network_update(&state, &b, &rate) == 0 && rate.received == 4);
    a = b; a.time++; a.count = 2;
    strcpy(a.entries[1].name, "en2"); a.entries[1].index = 4;
    CHECK(st_network_update(&state, &a, &rate) != 0);
    b = a; b.time++; b.entries[0].received += 1; b.entries[1].received += 2;
    CHECK(st_network_update(&state, &b, &rate) == 0 && rate.received == 3);
    a = b; a.time++; a.entries[0] = b.entries[1]; a.entries[1] = b.entries[0];
    a.entries[0].received += 2; a.entries[1].received += 1;
    CHECK(st_network_update(&state, &a, &rate) == 0 && rate.received == 3);
    b = a; b.time++; b.count = 1;
    CHECK(st_network_update(&state, &b, &rate) != 0);
    puts("PASS interface churn and ordering");
    a = b; a.time = b.time; a.entries[0].received++;
    CHECK(st_network_update(&state, &a, &rate) != 0);
    b = a; b.time++; b.entries[0].received++;
    CHECK(st_network_update(&state, &b, &rate) == 0 && rate.received == 1);
    a = b; a.time += ST_NETWORK_MAX_INTERVAL + 1;
    CHECK(st_network_update(&state, &a, &rate) != 0);
    b = a; b.time++; CHECK(st_network_update(&state, &b, &rate) == 0);
    a = b; a.time = NAN; CHECK(st_network_update(&state, &a, &rate) != 0);
    b.time++; CHECK(st_network_update(&state, &b, &rate) != 0);
    b.time++; CHECK(st_network_update(&state, &b, &rate) == 0);
    a = b; a.time++; a.count = 0;
    CHECK(st_network_update(&state, &a, &rate) != 0);
    b.time += 2; CHECK(st_network_update(&state, &b, &rate) != 0);
    b.time++; CHECK(st_network_update(&state, &b, &rate) == 0);
    CHECK(st_network_update(&state, NULL, &rate) != 0);
    b.time++; CHECK(st_network_update(&state, &b, &rate) != 0);
    b.time++; CHECK(st_network_update(&state, &b, &rate) == 0);
    puts("PASS invalid time, empty set and read error");
    {
        char text[128];
        rate.received = 1024; rate.sent = 512;
        CHECK(st_format_network(text, sizeof(text), &rate) == 0);
        CHECK(strcmp(text, "Network: down 1.0 / up 0.5 KiB/s") == 0);
        strcpy(text, "unchanged");
        CHECK(st_format_network(text, 3, &rate) != 0 && strcmp(text, "unchanged") == 0);
        rate.received = NAN;
        CHECK(st_format_network(text, sizeof(text), &rate) != 0);
    }
    puts("PASS network formatting");
    memset(&state, 0, sizeof(state)); memset(&a, 0, sizeof(a));
    a.count = 1; strcpy(a.entries[0].name, "en0"); a.entries[0].index = 1; a.time = DBL_MIN;
    CHECK(st_network_update(&state, &a, &rate) != 0);
    b = a; b.time *= 2; b.entries[0].received = UINT64_MAX;
    rate.received = 987; rate.sent = 654;
    CHECK(st_network_update(&state, &b, &rate) != 0 && rate.received == 987 && rate.sent == 654);
    puts("PASS rate overflow is unavailable");
    memset(&state, 0, sizeof(state));
    memset(&a, 0, sizeof(a)); a.count = 1; a.time = 100;
    strcpy(a.entries[0].name, "en0"); a.entries[0].index = 1; a.entries[0].sent = 100;
    CHECK(st_network_update(&state, &a, &rate) != 0);
    b = a; b.time++; b.entries[0].sent = 99;
    CHECK(st_network_update(&state, &b, &rate) != 0);
    a = b; a.time--; CHECK(st_network_update(&state, &a, &rate) != 0);
    b = a; b.time += ST_NETWORK_MAX_INTERVAL;
    CHECK(st_network_update(&state, &b, &rate) == 0);
    a = b; a.time++; a.count = 2; a.entries[1] = a.entries[0];
    CHECK(st_network_update(&state, &a, &rate) != 0);
    b.time += 2; CHECK(st_network_update(&state, &b, &rate) != 0);
    a = b; a.time = INFINITY; CHECK(st_network_update(&state, &a, &rate) != 0);
    CHECK(st_network_update(NULL, &b, &rate) != 0);
    puts("PASS outbound decrease, backwards time and malformed set");
    return 0;
}
