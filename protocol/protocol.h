#ifndef PROTOCOL_H
#define PROTOCOL_H

#define TARGET_PORT 		3134
#define CLIENTS_IFACE_ADDR	INADDR_ANY
#define TARGET_ADDR 		"127.0.0.1"

#define MAGIC_START_KEY	0xDEAD8086
#define MAGIC_END_KEY	0x8086BEAF

#define BUFFER_SIZE		1024
#define Z_CHUNK		BUFFER_SIZE

typedef enum possibleCompressions
{
	noCompression,
	zlibDeflate,
	zlibInflate,
	lzmaCompress,
	lzmaDeCompress,
	gzCompress,
	gzDeCompress,
	unsuppportedCompression
} tCompression;

typedef struct __attribute__((__packed__)) Ack_t {
	int start_key;
	int end_key;
} Ack;

typedef struct __attribute__((__packed__)) MagicToken_t {
	int start_key;
	tCompression compressionType;
	unsigned int nextdatasizes;
	unsigned char workModel;
	int end_key;
} MagicToken;

int isHeaderValid(char *input, unsigned int len);
unsigned int getFilesizeFromHeader(char *input);
unsigned int getCompressionFromHeader(char *input);
#endif //_PROTOCOL_H_
