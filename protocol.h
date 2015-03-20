#ifndef PROTOCOL_H
#define PROTOCOL_H

#define	DEFAULT_PORT	60000

enum {
	CODE_DATA=0,
	CODE_HELLO,
	CODE_PING,
	CODE_PONG,
	CODE_FEC_SEG,
};

struct frame_st {
	uint8_t code;
	union {
		struct {
			uint8_t recver_tun_addr[16];
			uint8_t sender_tun_addr[16];
			uint32_t salt;
			uint32_t crc32;
		} hello_body;
		struct {
			uint64_t timestamp_ms;
		} echo_body;
		struct {
			uint8_t data[0];
		} data_body;
		struct {
			uint64_t fecg_id;
			uint8_t fecg_seq;
			uint8_t fecg_size;
			uint8_t fecg_seg[0];
		} fec_segment;
	};
};

#endif

