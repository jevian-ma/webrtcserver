#include "rtp.h"

int is_dtls(char *buf) {
    return ((*buf >= 20) && (*buf <= 64));
}

int is_rtp(char *buf) {
    struct RTP_HEADER *header = (struct RTP_HEADER*)buf;
    return ((header->type < 64) || (header->type >= 96));
}

int is_rtcp(char *buf) {
    struct RTP_HEADER *header = (struct RTP_HEADER*)buf;
    return ((header->type >= 64) && (header->type < 96));
}