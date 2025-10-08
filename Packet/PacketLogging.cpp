#include"PacketLogging.h"
#include"PacketQueue.h"

//DWORD packet_id_out = (GetCurrentProcessId() << 16); // 偶数
//DWORD packet_id_in = (GetCurrentProcessId() << 16) + 1; // 奇数
DWORD packet_id_out = 2; // 偶数
DWORD packet_id_in = 1; // 奇数

// Global setting for packet blocking (false = async logging only, true = wait for block check)
bool g_EnableBlocking = false;

DWORD CountUpPacketID(DWORD &id) {
	id += 2;
	return id;
}

bool InPacketLogging(MessageHeader type, InPacket *ip, void *retAddr) {
	switch (type) {
	case ENCODE_BEGIN:
	{
		// count up
		// tracking start
		return true;
	}
	case ENCODE_END:
	{
		// notify encoded size
		return true;
	}
	default: {
		break;
	}
	}
	return false;
}

bool OutPacketLogging(MessageHeader type, OutPacket *op, void *retAddr) {
	switch (type) {
	case DECODE_BEGIN:
	{
		// count up
		// tracking start
		return true;
	}
	case DECODE_END:
	{
		// notify decoded size
		return true;
	}
	default: {
		break;
	}
	}
	return false;
}


void AddExtra(PacketExtraInformation &pxi) {
	if (!g_BufferPool || !g_PacketQueue) {
		return; // Queue not initialized
	}

	size_t total_size = sizeof(PacketEditorMessage) + pxi.size;
	size_t buffer_index;
	BYTE* b = g_BufferPool->Allocate(total_size, buffer_index);

	if (!b) {
		return;
	}

	union {
		PacketEditorMessage *pem;
		BYTE *pb;
	};
	pb = b;

	pem->header = pxi.fmt;
	pem->id = pxi.id;
	pem->addr = pxi.addr;
	pem->Extra.pos = pxi.pos;
	pem->Extra.size = pxi.size;

	if (!pxi.data) {
		pem->Extra.update = FORMAT_NO_UPDATE;
	}
	else {
		pem->Extra.update = FORMAT_UPDATE;
		memcpy_s(&pem->Extra.data[0], pxi.size, &pxi.data[0], pxi.size);
	}

	// Queue async (no response needed for format info)
	g_PacketQueue->QueuePacket(b, total_size, buffer_index);
}

// for SendPacket format - using unordered_map for O(1) lookup
std::unordered_map<ULONG_PTR, std::vector<PacketExtraInformation>> packet_tracking_map;
CRITICAL_SECTION tracking_cs;
bool tracking_cs_initialized = false;

void InitTracking() {
	if (!tracking_cs_initialized) {
		InitializeCriticalSection(&tracking_cs);
		tracking_cs_initialized = true;
	}
}

void ClearQueue(OutPacket *op) {
	if (!tracking_cs_initialized) return;

	ULONG_PTR tracking_id = (ULONG_PTR)op;
	EnterCriticalSection(&tracking_cs);
	auto it = packet_tracking_map.find(tracking_id);
	if (it != packet_tracking_map.end()) {
		packet_tracking_map.erase(it);
	}
	LeaveCriticalSection(&tracking_cs);
}

void AddQueue(PacketExtraInformation &pxi) {
	if (!tracking_cs_initialized) return;

	//DEBUG(L"debug... ID : " + std::to_wstring(pxi.id) + L", " + std::to_wstring(pxi.pos) + L", " + std::to_wstring(pxi.size));
	ULONG_PTR tracking_id = pxi.tracking;

	EnterCriticalSection(&tracking_cs);
	packet_tracking_map[tracking_id].push_back(pxi);
	LeaveCriticalSection(&tracking_cs);
}

void AddExtraAll(OutPacket *op) {
	if (!tracking_cs_initialized) return;

	ULONG_PTR tracking_id = (ULONG_PTR)op;

	EnterCriticalSection(&tracking_cs);
	auto it = packet_tracking_map.find(tracking_id);
	if (it != packet_tracking_map.end()) {
		// Copy and erase before leaving critical section
		std::vector<PacketExtraInformation> items = std::move(it->second);
		packet_tracking_map.erase(it);
		LeaveCriticalSection(&tracking_cs);

		// Send outside critical section
		for (auto &pei : items) {
			pei.id = packet_id_out; // fix
			AddExtra(pei);
		}
	} else {
		LeaveCriticalSection(&tracking_cs);
	}
	//DEBUG(L"list_st = " + std::to_wstring(packet_tracking_map.size()));
}

void AddSendPacket(OutPacket *op, ULONG_PTR addr, bool &bBlock) {
	AddExtraAll(op);

	if (!g_BufferPool || !g_PacketQueue) {
		return; // Queue not initialized
	}

	size_t total_size = sizeof(PacketEditorMessage) + op->encoded;
	size_t buffer_index;
	BYTE* b = g_BufferPool->Allocate(total_size, buffer_index);

	if (!b) {
		return;
	}

	union {
		PacketEditorMessage *pem;
		BYTE *pb;
	};
	pb = b;

	pem->header = SENDPACKET;
	pem->id = packet_id_out;
	pem->addr = addr;
	pem->Binary.length = op->encoded;
	memcpy_s(pem->Binary.packet, op->encoded, op->packet, op->encoded);
	CountUpPacketID(packet_id_out); // SendPacketとEnterSendPacketがあるのでここでカウントアップ

#ifdef _WIN64
	if (op->header) {
		*(WORD *)&pem->Binary.packet[0] = op->header;
	}
#endif

	bBlock = false;

	// If blocking enabled, wait for response. Otherwise send async (much faster!)
	if (g_EnableBlocking) {
		g_PacketQueue->QueuePacketBlocking(b, total_size, buffer_index, bBlock);
	} else {
		g_PacketQueue->QueuePacket(b, total_size, buffer_index);
	}
}

void AddRecvPacket(InPacket *ip, ULONG_PTR addr, bool &bBlock) {
	if (!g_BufferPool || !g_PacketQueue) {
		return; // Queue not initialized
	}

	size_t total_size = sizeof(PacketEditorMessage) + ip->size;
	size_t buffer_index;
	BYTE* b = g_BufferPool->Allocate(total_size, buffer_index);

	if (!b) {
		return;
	}

	union {
		PacketEditorMessage *pem;
		BYTE *pb;
	};
	pb = b;

	pem->header = RECVPACKET;
	pem->id = packet_id_in;
	pem->addr = addr;
	pem->Binary.length = ip->size;
	memcpy_s(pem->Binary.packet, ip->size, &ip->packet[4], ip->size);

	bBlock = false;

	// If blocking enabled, wait for response. Otherwise send async (much faster!)
	if (g_EnableBlocking) {
		g_PacketQueue->QueuePacketBlocking(b, total_size, buffer_index, bBlock);
	} else {
		g_PacketQueue->QueuePacket(b, total_size, buffer_index);
	}
}

PipeClient *pc = NULL;

bool StartPipeClient() {
	InitTracking();
	pc = new PipeClient(GetPipeNameLogger());
	return pc->Run();
}

bool RestartPipeClient() {
	if (pc) {
		delete pc;
		pc = NULL;
	}
	return StartPipeClient();
}