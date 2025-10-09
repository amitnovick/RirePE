#pragma once
#include <Windows.h>

// Configuration
#define DLL_NAME L"Packet"
#define INI_FILE_NAME L"RirePE.ini"

// TCP Configuration
#define DEFAULT_TCP_PORT 8275

#pragma pack(push, 1)

// Message header types
enum MessageHeader {
	SENDPACKET,        // stop encoding
	RECVPACKET,        // start decoding
	// encode
	ENCODE_BEGIN,
	ENCODEHEADER,
	ENCODE1,
	ENCODE2,
	ENCODE4,
	ENCODE8,
	ENCODESTR,
	ENCODEBUFFER,
	TV_ENCODEHEADER,
	TV_ENCODESTRW1,
	TV_ENCODESTRW2,
	TV_ENCODEFLOAT,
	ENCODE_END,
	// decode
	DECODE_BEGIN,
	DECODEHEADER,
	DECODE1,
	DECODE2,
	DECODE4,
	DECODE8,
	DECODESTR,
	DECODEBUFFER,
	TV_DECODEHEADER,
	TV_DECODESTRW1,
	TV_DECODESTRW2,
	TV_DECODEFLOAT,
	DECODE_END,        // not a tag
	// unknown
	UNKNOWNDATA,       // not decoded by function
	NOTUSED,           // recv not used
	WHEREFROM,         // not encoded by function
	UNKNOWN,
};

enum FormatUpdate {
	FORMAT_NO_UPDATE,
	FORMAT_UPDATE,
};

// Packet editor message structure
typedef struct {
	MessageHeader header;
	DWORD id;
#ifdef _WIN64
	ULONG_PTR addr;
#else
	ULONGLONG addr;
#endif
	union {
		// SEND or RECV
		struct {
			DWORD length;     // packet size
			BYTE packet[1];   // packet data
		} Binary;
		// Encode or Decode
		struct {
			DWORD pos;        // encoded/decoded position
			DWORD size;       // size
			FormatUpdate update;
			BYTE data[1];     // packet buffer (may change before read)
		} Extra;
		// Encode or Decode completion
		DWORD status;         // status
	};
} PacketEditorMessage;

#pragma pack(pop)
