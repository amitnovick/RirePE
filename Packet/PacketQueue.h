#ifndef __PACKET_QUEUE_H__
#define __PACKET_QUEUE_H__

#include<Windows.h>
#include<queue>
#include"../RirePE/RirePE.h"

// Memory pool for packet buffers
class PacketBufferPool {
private:
	static const size_t POOL_SIZE = 64;
	static const size_t BUFFER_SIZE = 8192; // Max packet size

	struct Buffer {
		BYTE data[BUFFER_SIZE];
		size_t size;
		bool in_use;
	};

	Buffer buffers[POOL_SIZE];
	CRITICAL_SECTION cs;

public:
	PacketBufferPool();
	~PacketBufferPool();

	BYTE* Allocate(size_t size, size_t &buffer_index);
	void Free(size_t buffer_index);
};

// Async packet queue item
struct QueuedPacket {
	BYTE* data;
	size_t size;
	size_t buffer_index;
	bool needs_response;
	HANDLE response_event; // For blocking packets only
	bool block_result;
};

// Lock-free-ish async packet queue with background worker
class AsyncPacketQueue {
private:
	std::queue<QueuedPacket> packet_queue;
	CRITICAL_SECTION queue_cs;
	HANDLE worker_thread;
	HANDLE wake_event;
	volatile bool running;

	static DWORD WINAPI WorkerThreadProc(LPVOID param);
	void ProcessQueue();

public:
	AsyncPacketQueue();
	~AsyncPacketQueue();

	bool Start();
	void Stop();

	// Non-blocking send (for format info)
	bool QueuePacket(BYTE* data, size_t size, size_t buffer_index);

	// Blocking send (for send/recv packets that need block response)
	bool QueuePacketBlocking(BYTE* data, size_t size, size_t buffer_index, bool &block_result);
};

extern PacketBufferPool* g_BufferPool;
extern AsyncPacketQueue* g_PacketQueue;

bool InitializePacketQueue();
void ShutdownPacketQueue();

#endif
