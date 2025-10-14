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
	// New message types for queue configuration
	REGISTER_QUEUE,    // Register a new injection queue with configuration
	UNREGISTER_QUEUE,  // Remove a queue registration
	CLEAR_QUEUES,      // Clear all queue registrations
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

// Multi-packet queue configuration structures
// Sent by client to register a new injection queue that can handle multiple packets
#define MAX_QUEUE_NAME_LENGTH 32
#define MAX_TIMESTAMP_OFFSETS 8
#define MAX_PACKETS_PER_QUEUE 8

// Timestamp configuration for a single packet type
typedef struct {
	BYTE needs_timestamp_update;              // 1 if timestamp needs to be generated, 0 otherwise
	BYTE timestamp_offset_count;              // Number of timestamp offsets (0-8)
	DWORD timestamp_offsets[MAX_TIMESTAMP_OFFSETS];  // Offsets where timestamps should be written (little-endian DWORD)
	BYTE padding[2];                          // Padding for alignment
} PacketTimestampConfig;

// Queue configuration for multi-packet injection
typedef struct {
	char queue_name[MAX_QUEUE_NAME_LENGTH];  // Queue name (e.g., "GENERAL", "ATTACK", "ITEM_PICK_UP")
	DWORD injection_interval_ms;              // Injection interval in milliseconds (0 = no delay)
	BYTE packet_count;                        // Number of packets in this queue (1-8)
	BYTE padding[3];                          // Padding for alignment
	WORD packet_opcodes[MAX_PACKETS_PER_QUEUE];  // Packet opcodes in order (e.g., [0x002E, 0x00EF] for MOVE+PICKUP)
	PacketTimestampConfig timestamp_configs[MAX_PACKETS_PER_QUEUE];  // Timestamp config for each packet
} QueueConfigMessage;

#pragma pack(pop)
