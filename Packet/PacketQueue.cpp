#include"../Share/Simple/Simple.h"
#include"../Share/Simple/DebugLog.h"
#include"PacketDefs.h"
#include"PacketQueue.h"
#include"PacketLogging.h"

PacketBufferPool* g_BufferPool = NULL;
AsyncPacketQueue* g_PacketQueue = NULL;

// ============================================================================
// PacketBufferPool Implementation
// ============================================================================

PacketBufferPool::PacketBufferPool() {
	InitializeCriticalSection(&cs);
	for (size_t i = 0; i < POOL_SIZE; i++) {
		buffers[i].in_use = false;
		buffers[i].size = 0;
	}
}

PacketBufferPool::~PacketBufferPool() {
	DeleteCriticalSection(&cs);
}

BYTE* PacketBufferPool::Allocate(size_t size, size_t &buffer_index) {
	if (size > BUFFER_SIZE) {
		// Fallback to regular allocation for oversized packets
		static int oversized_count = 0;
		if (++oversized_count <= 5 || oversized_count % 100 == 0) {
			DEBUGLOG(L"[BUFFER] WARNING: Oversized packet (" + std::to_wstring(size) + L" bytes > " + std::to_wstring(BUFFER_SIZE) + L"), count: " + std::to_wstring(oversized_count));
		}
		buffer_index = (size_t)-1;
		return new BYTE[size];
	}

	EnterCriticalSection(&cs);
	for (size_t i = 0; i < POOL_SIZE; i++) {
		if (!buffers[i].in_use) {
			buffers[i].in_use = true;
			buffers[i].size = size;
			buffer_index = i;
			LeaveCriticalSection(&cs);
			return buffers[i].data;
		}
	}
	LeaveCriticalSection(&cs);

	// Pool exhausted, fallback to regular allocation
	static int exhaustion_count = 0;
	if (++exhaustion_count <= 10 || exhaustion_count % 50 == 0) {
		DEBUGLOG(L"[BUFFER] WARNING: Pool exhausted! Allocating from heap (count: " + std::to_wstring(exhaustion_count) + L")");
	}
	buffer_index = (size_t)-1;
	return new BYTE[size];
}

void PacketBufferPool::Free(size_t buffer_index) {
	if (buffer_index == (size_t)-1) {
		// Was allocated outside pool, caller must handle
		return;
	}

	EnterCriticalSection(&cs);
	if (buffer_index < POOL_SIZE) {
		buffers[buffer_index].in_use = false;
		buffers[buffer_index].size = 0;
	}
	LeaveCriticalSection(&cs);
}

// ============================================================================
// AsyncPacketQueue Implementation
// ============================================================================

AsyncPacketQueue::AsyncPacketQueue() {
	InitializeCriticalSection(&queue_cs);
	wake_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	worker_thread = NULL;
	running = false;
}

AsyncPacketQueue::~AsyncPacketQueue() {
	Stop();
	CloseHandle(wake_event);
	DeleteCriticalSection(&queue_cs);
}

bool AsyncPacketQueue::Start() {
	if (running) {
		return true;
	}

	running = true;
	worker_thread = CreateThread(NULL, 0, WorkerThreadProc, this, 0, NULL);
	return worker_thread != NULL;
}

void AsyncPacketQueue::Stop() {
	if (!running) {
		return;
	}

	running = false;
	SetEvent(wake_event);

	if (worker_thread) {
		WaitForSingleObject(worker_thread, 5000);
		CloseHandle(worker_thread);
		worker_thread = NULL;
	}

	// Clean up remaining queue items
	EnterCriticalSection(&queue_cs);
	while (!packet_queue.empty()) {
		QueuedPacket qp = packet_queue.front();
		packet_queue.pop();

		if (qp.buffer_index != (size_t)-1) {
			g_BufferPool->Free(qp.buffer_index);
		} else {
			delete[] qp.data;
		}

		if (qp.response_event) {
			CloseHandle(qp.response_event);
		}
	}
	LeaveCriticalSection(&queue_cs);
}

bool AsyncPacketQueue::QueuePacket(BYTE* data, size_t size, size_t buffer_index) {
	QueuedPacket qp;
	qp.data = data;
	qp.size = size;
	qp.buffer_index = buffer_index;
	qp.needs_response = false;
	qp.response_event = NULL;
	qp.block_result = false;

	EnterCriticalSection(&queue_cs);
	packet_queue.push(qp);
	size_t queue_size = packet_queue.size();
	LeaveCriticalSection(&queue_cs);

	// Warn if queue is backing up
	static size_t max_queue_size = 0;
	if (queue_size > max_queue_size) {
		max_queue_size = queue_size;
		if (max_queue_size > 10 && (max_queue_size % 10 == 0)) {
			DEBUGLOG(L"[QUEUE] WARNING: Queue depth reached " + std::to_wstring(max_queue_size) + L" packets!");
		}
	}

	SetEvent(wake_event);
	return true;
}

bool AsyncPacketQueue::QueuePacketBlocking(BYTE* data, size_t size, size_t buffer_index, bool &block_result) {
	QueuedPacket qp;
	qp.data = data;
	qp.size = size;
	qp.buffer_index = buffer_index;
	qp.needs_response = true;
	qp.response_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	qp.block_result = false;

	if (!qp.response_event) {
		if (buffer_index != (size_t)-1) {
			g_BufferPool->Free(buffer_index);
		} else {
			delete[] data;
		}
		return false;
	}

	EnterCriticalSection(&queue_cs);
	packet_queue.push(qp);
	LeaveCriticalSection(&queue_cs);

	SetEvent(wake_event);

	// Wait for response
	WaitForSingleObject(qp.response_event, INFINITE);
	block_result = qp.block_result;
	CloseHandle(qp.response_event);

	return true;
}

DWORD WINAPI AsyncPacketQueue::WorkerThreadProc(LPVOID param) {
	AsyncPacketQueue* queue = (AsyncPacketQueue*)param;
	queue->ProcessQueue();
	return 0;
}

void AsyncPacketQueue::ProcessQueue() {
	const int BATCH_SIZE = 16; // Process up to 16 packets at once
	int processed = 0;

	while (running) {
		WaitForSingleObject(wake_event, 10); // 10ms timeout for shutdown check (faster response)

		processed = 0;
		while (running && processed < BATCH_SIZE) {
			QueuedPacket qp;
			bool has_packet = false;

			EnterCriticalSection(&queue_cs);
			if (!packet_queue.empty()) {
				qp = packet_queue.front();
				packet_queue.pop();
				has_packet = true;
			}
			LeaveCriticalSection(&queue_cs);

			if (!has_packet) {
				break;
			}

			processed++;

			// Send packet through pipe or TCP
			bool success = false;
			if (SendPacketData(qp.data, qp.size)) {
				success = true;

				// Determine if this packet type expects a response
				// SENDPACKET and RECVPACKET always get responses from RirePE.exe
				bool expects_response = false;
				if (qp.size >= sizeof(PacketEditorMessage)) {
					PacketEditorMessage* pem = (PacketEditorMessage*)qp.data;
					expects_response = (pem->header == SENDPACKET || pem->header == RECVPACKET);
				}

				// Always read response if server sends one (to keep communication in sync)
				if (expects_response) {
					BYTE response = 0;
					std::vector<BYTE> vData;
					if (RecvPacketData(vData) && vData.size() >= 1) {
						response = vData[0];
						// Only store result if caller is waiting
						if (qp.needs_response) {
							qp.block_result = (response == 1);
						}
					}
				}
			} else {
				// Connection failed - don't restart on every packet failure
				// The startup code will handle initial connection, and we'll just skip failed packets
				// to avoid spamming restart attempts
				static int failure_count = 0;
				failure_count++;

				// Only attempt restart after many consecutive failures (not on every packet)
				if (failure_count == 50) {
					extern bool g_UseTCP;
					// Always try to restart pipe first since that's for RirePE.exe
					RestartPipeClient();
					failure_count = 0; // Reset counter
				}
			}

			// Free buffer
			if (qp.buffer_index != (size_t)-1) {
				g_BufferPool->Free(qp.buffer_index);
			} else {
				delete[] qp.data;
			}

			// Signal response event if needed
			if (qp.response_event) {
				SetEvent(qp.response_event);
			}
		}
	}
}

// ============================================================================
// Global initialization
// ============================================================================

bool InitializePacketQueue() {
	g_BufferPool = new PacketBufferPool();
	g_PacketQueue = new AsyncPacketQueue();

	if (!g_BufferPool || !g_PacketQueue) {
		return false;
	}

	return g_PacketQueue->Start();
}

void ShutdownPacketQueue() {
	if (g_PacketQueue) {
		delete g_PacketQueue;
		g_PacketQueue = NULL;
	}

	if (g_BufferPool) {
		delete g_BufferPool;
		g_BufferPool = NULL;
	}
}
