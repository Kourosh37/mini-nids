/* Deterministic capture ABI fixture; never opens a network interface. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#define EXPORT __declspec(dllexport)
#else
#include <sys/time.h>
#define EXPORT __attribute__((visibility("default")))
#endif
struct Header {
    struct timeval time;
    uint32_t captured, original;
};
struct Device {
    struct Device *next;
    char *name, *description;
    void *addresses;
    uint32_t flags;
};
struct Capture {
    unsigned index;
    int mode;
    struct Header header;
    unsigned char packet[42];
};
EXPORT int pcap_findalldevs(struct Device **devices, char *error) {
    static struct Device device = {0, "fixture", "Deterministic test backend; no real traffic", 0, 0};
    (void)error;
    *devices = &device;
    return 0;
}
EXPORT void pcap_freealldevs(struct Device *devices) {
    (void)devices;
}
EXPORT void *pcap_open_live(const char *name, int snaplen, int promisc, int timeout, char *error) {
    struct Capture *s;
    (void)snaplen;
    (void)promisc;
    (void)timeout;
    if (!strcmp(name, "fail")) {
        strcpy(error, "fixture permission denied");
        return 0;
    }
    s = (struct Capture *)calloc(1, sizeof(*s));
    if (!s)
        return 0;
    s->mode = !strcmp(name, "idle") ? 1 : !strcmp(name, "badlink") ? 2 : !strcmp(name, "badrecord") ? 3 : 0;
    return s;
}
EXPORT void pcap_close(void *p) {
    free(p);
}
EXPORT int pcap_datalink(void *p) {
    return ((struct Capture *)p)->mode == 2 ? 113 : 1;
}
EXPORT int pcap_setnonblock(void *p, int enabled, char *error) {
    (void)p;
    (void)enabled;
    (void)error;
    return 0;
}
EXPORT char *pcap_geterr(void *p) {
    (void)p;
    return "fixture read error";
}
EXPORT int pcap_stats(void *p, unsigned *stats) {
    stats[0] = ((struct Capture *)p)->index;
    stats[1] = 7;
    stats[2] = 2;
    return 0;
}
EXPORT int pcap_next_ex(void *p, struct Header **header, const unsigned char **bytes) {
    struct Capture *s = (struct Capture *)p;
    unsigned char id;
    if (s->mode == 1 || s->index == 3)
        return 0;
    ++s->index;
    id = s->index == 1 ? 1 : 2;
    memset(s->packet, 0, sizeof(s->packet));
    s->packet[0] = 2;
    s->packet[5] = 1;
    s->packet[6] = 2;
    s->packet[11] = id;
    s->packet[12] = 8;
    s->packet[13] = 6;
    s->packet[15] = 1;
    s->packet[16] = 8;
    s->packet[18] = 6;
    s->packet[19] = 4;
    s->packet[21] = 2;
    s->packet[22] = 2;
    s->packet[27] = id;
    s->packet[28] = 192;
    s->packet[30] = 2;
    s->packet[31] = 2;
    s->packet[32] = 2;
    s->packet[37] = 1;
    s->packet[38] = 192;
    s->packet[40] = 2;
    s->packet[41] = 1;
    s->header.time.tv_sec = (long)time(0);
    s->header.time.tv_usec = (long)(s->index * 100);
    s->header.captured = 42;
    s->header.original = 42;
    if (s->mode == 3)
        s->header.captured = 300000;
    *header = &s->header;
    *bytes = s->packet;
    return 1;
}
