#ifndef __RTP_H__
#define __RTP_H__

#ifdef __MACH__
#include <machine/endian.h>
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif
#include <stdint.h>

struct RTP_HEADER
{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint16_t version:2;
	uint16_t padding:1;
	uint16_t extension:1;
	uint16_t csrccount:4;
	uint16_t markerbit:1;
	uint16_t type:7;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	uint16_t csrccount:4;
	uint16_t extension:1;
	uint16_t padding:1;
	uint16_t version:2;
	uint16_t type:7;
	uint16_t markerbit:1;
#endif
	uint16_t seq_number;
	uint32_t timestamp;
	uint32_t ssrc;
	uint32_t csrc[16];
};

int is_dtls(char *buf);
int is_rtp(char *buf);
int is_rtcp(char *buf);

#endif
