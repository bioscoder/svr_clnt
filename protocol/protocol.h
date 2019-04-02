#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#define TARGET_PORT 		3134
#define CLIENTS_IFACE_ADDR	INADDR_ANY
#define TARGET_ADDR 		"127.0.0.1"

#define MAGIC_START_KEY	0xDEAD8086
#define MAGIC_END_KEY	0x8086BEAF

#define MAX_BUFFER_SIZE		32*1024

typedef enum possibleCompressions
{
	noCompression = 0,
	rawDeflate,
	rawInflate,
	zlibDeflate,
	zlibInflate,
	lzmaCompress,
	lzmaDeCompress,
	gzCompress,
	gzDeCompress,
	unsuppportedCompression
} tCompression;

#pragma pack(push,1)
typedef struct Ack_t {
	int start_key;
	int end_key;
} Ack;

typedef struct MagicToken_t {
	int start_key;
	tCompression compressionType;
	int compressionLevel;
	unsigned int chunk_size;
	unsigned int nextdatasizes;
	unsigned char workModel;
	int err_code;
	int end_key;
} MagicToken;
#pragma pack(pop)
int isHeaderValid(char *input, unsigned int len);
#endif //_PROTOCOL_H_
