// PacketSender.cpp - Multi-packet dynamic queue-based injection system
#include"../Share/Simple/Simple.h"
#include"../Share/Hook/SimpleHook.h"
#include"../Packet/PacketHook.h"
#include"PacketDefs.h"
#include"../Share/Simple/DebugLog.h"
#include <queue>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

bool bInjectorCallback = false;

// Timestamp configuration for a single packet
struct TimestampConfig {
	bool needs_update;
	std::vector<DWORD> offsets;
};

// Dynamic queue configuration structure
struct QueueConfig {
	std::string queue_name;
	DWORD injection_interval_ms;
	DWORD last_injection_time_ms;
	BYTE packet_count;                       // Number of packets expected in each group
	std::vector<TimestampConfig> timestamp_configs;  // Timestamp config for each packet
	std::vector<DWORD> packet_intervals_ms;  // Delay BEFORE injecting each packet (in ms)
};

// Multi-packet group waiting to be injected
struct MultiPacketGroup {
	std::vector<std::vector<BYTE>> packets;  // Multiple packets to inject together
	DWORD queued_time_ms;
	BYTE current_packet_index;               // Index of next packet to inject (0-based)
	DWORD next_packet_time_ms;               // When the next packet should be injected
};

// Map from queue name to queue configuration
std::map<std::string, QueueConfig> queue_configs;

// Map from queue name to packet queue
std::map<std::string, std::queue<MultiPacketGroup>> packet_queues;

// Temporary storage for incomplete multi-packet groups being assembled
struct IncompleteGroup {
	std::vector<std::vector<BYTE>> packets;
	DWORD start_time_ms;
};
std::map<std::string, IncompleteGroup> incomplete_groups;

CRITICAL_SECTION injection_queue_cs;
bool injection_queue_initialized = false;

// Helper function to get current time in milliseconds (using GetTickCount)
inline DWORD GetCurrentTimeMs() {
	return GetTickCount();
}

// Register a new queue configuration
bool RegisterQueue(const QueueConfigMessage& config) {
	if (!injection_queue_initialized) {
		InitializeCriticalSection(&injection_queue_cs);
		injection_queue_initialized = true;
	}

	EnterCriticalSection(&injection_queue_cs);

	// Create queue config
	QueueConfig qc;
	qc.queue_name = std::string(config.queue_name, strnlen(config.queue_name, MAX_QUEUE_NAME_LENGTH));
	qc.injection_interval_ms = config.injection_interval_ms;
	qc.packet_count = config.packet_count;
	// Initialize to current time so first injection waits for the full interval
	qc.last_injection_time_ms = GetCurrentTimeMs();

	// Copy timestamp configs and packet intervals
	for (BYTE i = 0; i < config.packet_count && i < MAX_PACKETS_PER_QUEUE; i++) {
		TimestampConfig tc;
		tc.needs_update = config.timestamp_configs[i].needs_timestamp_update != 0;
		for (BYTE j = 0; j < config.timestamp_configs[i].timestamp_offset_count && j < MAX_TIMESTAMP_OFFSETS; j++) {
			tc.offsets.push_back(config.timestamp_configs[i].timestamp_offsets[j]);
		}
		qc.timestamp_configs.push_back(tc);
		qc.packet_intervals_ms.push_back(config.packet_intervals_ms[i]);
	}

	// Register the queue
	queue_configs[qc.queue_name] = qc;

	// Initialize empty queue if not exists
	if (packet_queues.find(qc.queue_name) == packet_queues.end()) {
		packet_queues[qc.queue_name] = std::queue<MultiPacketGroup>();
	}

	LeaveCriticalSection(&injection_queue_cs);

	std::wstring queue_name_w(qc.queue_name.begin(), qc.queue_name.end());
	DEBUGLOG(L"[QUEUE] Registered multi-packet queue: " + queue_name_w +
		L" (packet_count=" + std::to_wstring(qc.packet_count) +
		L", interval=" + std::to_wstring(config.injection_interval_ms) + L"ms)");

	// Log each packet slot in the queue
	for (size_t i = 0; i < qc.timestamp_configs.size(); i++) {
		DEBUGLOG(L"[QUEUE]   Packet " + std::to_wstring(i + 1) +
			L": needs_timestamp=" + std::to_wstring(qc.timestamp_configs[i].needs_update) +
			L", timestamp_offsets=" + std::to_wstring(qc.timestamp_configs[i].offsets.size()) +
			L", delay=" + std::to_wstring(qc.packet_intervals_ms[i]) + L"ms");
	}

	return true;
}

// Unregister a queue by name
bool UnregisterQueue(const std::string& queue_name) {
	EnterCriticalSection(&injection_queue_cs);

	auto config_it = queue_configs.find(queue_name);
	if (config_it != queue_configs.end()) {
		queue_configs.erase(config_it);

		// Clear the queue
		auto queue_it = packet_queues.find(queue_name);
		if (queue_it != packet_queues.end()) {
			while (!queue_it->second.empty()) {
				queue_it->second.pop();
			}
			packet_queues.erase(queue_it);
		}

		// Clear incomplete group if exists
		incomplete_groups.erase(queue_name);

		LeaveCriticalSection(&injection_queue_cs);

		std::wstring queue_name_w(queue_name.begin(), queue_name.end());
		DEBUGLOG(L"[QUEUE] Unregistered queue: " + queue_name_w);
		return true;
	}

	LeaveCriticalSection(&injection_queue_cs);
	return false;
}

// Clear all queue configurations
void ClearAllQueues() {
	EnterCriticalSection(&injection_queue_cs);

	// Clear all queues
	for (auto& kv : packet_queues) {
		while (!kv.second.empty()) {
			kv.second.pop();
		}
	}

	packet_queues.clear();
	queue_configs.clear();
	incomplete_groups.clear();

	LeaveCriticalSection(&injection_queue_cs);

	DEBUGLOG(L"[QUEUE] Cleared all queue registrations");
}

// Helper function to inject a single packet (extracted from PacketInjector for reuse)
void InjectSinglePacket(std::vector<BYTE>& data) {
	PacketEditorMessage *pcm = (PacketEditorMessage *)&data[0];

	if (pcm->header == SENDPACKET) {
#ifdef _WIN64
		WORD wHeader = *(WORD *)&pcm->Binary.packet[0];
		OutPacket p;
		memset(&p, 0, sizeof(p));
		COutPacket_Hook(&p, wHeader);

		WORD wEncryptedHeader = *(WORD *)&p.packet[0];
		p.encoded = (DWORD)pcm->Binary.length;
#if MAPLE_VERSION <= 414
		p.packet = &pcm->Binary.packet[0];
#else
		memcpy_s(&p.packet[0], pcm->Binary.length, &pcm->Binary.packet[0], pcm->Binary.length);
#endif
		if (wHeader != wEncryptedHeader) {
			*(WORD *)&p.packet[0] = wEncryptedHeader;
			SendPacket_EH_Hook(&p);
		}
		else {
			SendPacket_Hook(_CClientSocket(), &p);
		}
#else
		extern void (*_EnterSendPacket_Original)(OutPacket *op);

		if (_EnterSendPacket_Original == NULL) {
			DEBUGLOG(L"InjectSinglePacket: CRITICAL ERROR - _EnterSendPacket_Original is NULL!");
			return;
		}

		// Initialize OutPacket structure with header using COutPacket
		WORD wHeader = *(WORD *)&pcm->Binary.packet[0];
		OutPacket p;
		memset(&p, 0, sizeof(p));
		COutPacket_Hook(&p, 0, wHeader);

		// Set up the packet data
		p.packet = &pcm->Binary.packet[0];
		p.encoded = pcm->Binary.length;

		_EnterSendPacket_Original(&p);
#endif
	}
	else if (pcm->header == RECVPACKET) {
		std::vector<BYTE> packet;
		packet.resize(pcm->Binary.length + 0x04);
		packet[0] = 0xF7;
		packet[1] = 0x39;
		packet[2] = 0xEF;
		packet[3] = 0x39;
		memcpy_s(&packet[4], pcm->Binary.length, &pcm->Binary.packet[0], pcm->Binary.length);
#ifdef _WIN64
		WORD wHeader = *(WORD *)&pcm->Binary.packet[0];
		wHeader = *(WORD *)&packet[0];
		InPacket p = { 0x00, 0x02, &packet[0], (DWORD)packet.size(), wHeader, (DWORD)pcm->Binary.length, 0x04 };
		ProcessPacket_Hook(_CClientSocket(), &p);
#else
		InPacket p = { 0x00, 0x02, &packet[0], (WORD)packet.size(), 0x00, (WORD)pcm->Binary.length, 0x00, 0x04 };
		ProcessPacket_Hook((void *)GetCClientSocket(), 0, &p);
#endif
	}
}

VOID CALLBACK PacketInjector(HWND, UINT, UINT_PTR, DWORD) {
	static int call_count = 0;
	call_count++;

	// Initialize critical section on first call
	if (!injection_queue_initialized) {
		InitializeCriticalSection(&injection_queue_cs);
		injection_queue_initialized = true;
	}

	DWORD current_time_ms = GetCurrentTimeMs();

	if (call_count % 100 == 1) {
		DEBUGLOG(L"[INJECT-TIMER] Tick #" + std::to_wstring(call_count) + L" at " + std::to_wstring(current_time_ms));
	}

	// Process packets from all registered queues
	// Priority: queues with shortest intervals first

	EnterCriticalSection(&injection_queue_cs);

	// Build a list of queues that are ready to inject (have packets and interval elapsed)
	std::vector<std::string> ready_queue_names;

	for (auto& config_kv : queue_configs) {
		const std::string& queue_name = config_kv.first;
		QueueConfig& config = config_kv.second;

		// Check if this queue has complete multi-packet groups
		auto queue_it = packet_queues.find(queue_name);
		if (queue_it == packet_queues.end() || queue_it->second.empty()) {
			continue;
		}

		// Check if enough time has passed since the packet was queued
		// This ensures minimum delay between when packet arrives and when it's injected
		MultiPacketGroup& front_group = queue_it->second.front();
		DWORD time_since_queued = current_time_ms - front_group.queued_time_ms;

		// Check if enough time has passed since last injection
		DWORD time_since_last = current_time_ms - config.last_injection_time_ms;

		// For multi-packet groups, check if enough time has passed for the NEXT packet
		bool next_packet_ready = (current_time_ms >= front_group.next_packet_time_ms);

		// Different conditions based on whether this is the first packet or a subsequent packet
		bool is_first_packet = (front_group.current_packet_index == 0);

		bool ready = false;
		if (is_first_packet) {
			// First packet: must satisfy inter-group delay (from last packet of previous group)
			// AND the first packet's own delay (packet_intervals_ms[0])
			// Note: time_since_queued handles the delay from when group arrived
			// time_since_last handles the delay from last injection (inter-group delay)
			ready = (time_since_last >= config.injection_interval_ms && next_packet_ready);
		} else {
			// Subsequent packets: only check per-packet timing
			// No inter-group delay, just the intra-group per-packet delay
			ready = next_packet_ready;
		}

		if (ready) {
			ready_queue_names.push_back(queue_name);
			// Always log for DIRECT queue, otherwise only every 25 ticks
			if (queue_name == "DIRECT" || call_count % 25 == 1) {
				std::wstring qname_w(queue_name.begin(), queue_name.end());
				DEBUGLOG(L"[INJECT-READY] Queue '" + qname_w + L"' ready (depth=" +
					std::to_wstring(queue_it->second.size()) +
					L", queued=" + std::to_wstring(time_since_queued) + L"ms" +
					L", last=" + std::to_wstring(time_since_last) + L"ms)");
			}
		}
	}

	// If no queues are ready, exit early
	if (ready_queue_names.empty()) {
		LeaveCriticalSection(&injection_queue_cs);
		return;
	}

	if (call_count % 25 == 1) {
		DEBUGLOG(L"[INJECT-START] Processing " + std::to_wstring(ready_queue_names.size()) + L" ready queue(s)");
	}

	// Sort by interval (shorter intervals = higher priority)
	std::sort(ready_queue_names.begin(), ready_queue_names.end(),
		[](const std::string& a, const std::string& b) {
			return queue_configs[a].injection_interval_ms < queue_configs[b].injection_interval_ms;
		});

	// Process ALL ready queues (not just one!) to maximize throughput
	// Limit to max 10 queues per tick to avoid blocking too long
	size_t max_queues_per_tick = min(ready_queue_names.size(), 10);

	std::vector<std::tuple<std::string, QueueConfig, MultiPacketGroup, size_t>> groups_to_inject;

	for (size_t q = 0; q < max_queues_per_tick; q++) {
		std::string queue_name = ready_queue_names[q];
		QueueConfig& config = queue_configs[queue_name];
		MultiPacketGroup group = packet_queues[queue_name].front();
		packet_queues[queue_name].pop();
		size_t remaining = packet_queues[queue_name].size();

		groups_to_inject.push_back(std::make_tuple(queue_name, config, group, remaining));
	}

	LeaveCriticalSection(&injection_queue_cs);

	// Now inject all groups outside the critical section
	DWORD new_timestamp = GetCurrentTimeMs();

	for (auto& tuple : groups_to_inject) {
		std::string queue_name = std::get<0>(tuple);
		QueueConfig config = std::get<1>(tuple);
		MultiPacketGroup group = std::get<2>(tuple);
		size_t remaining = std::get<3>(tuple);

		// Get the current packet index to inject
		BYTE packet_idx = group.current_packet_index;

		// Update timestamp for the current packet only
		if (packet_idx < config.timestamp_configs.size()) {
			TimestampConfig& ts_config = config.timestamp_configs[packet_idx];
			if (ts_config.needs_update) {
				PacketEditorMessage *pcm = (PacketEditorMessage *)&group.packets[packet_idx][0];
				if (pcm->header == SENDPACKET) {
					// Update all configured timestamp offsets
					for (DWORD offset : ts_config.offsets) {
						if (offset + 4 <= pcm->Binary.length) {
							*(DWORD *)&pcm->Binary.packet[offset] = new_timestamp;
						}
					}
				}
			}
		}

		// Log injection (always log for DIRECT queue, otherwise only every 25 ticks)
		if (queue_name == "DIRECT" || call_count % 25 == 1) {
			std::wstring qname_w(queue_name.begin(), queue_name.end());
			DEBUGLOG(L"[INJECT-DO] Injecting packet " + std::to_wstring(packet_idx + 1) +
				L"/" + std::to_wstring(group.packets.size()) +
				L" from '" + qname_w + L"' (remaining=" +
				std::to_wstring(remaining) + L" groups)");
		}

		// Inject the current packet
		InjectSinglePacket(group.packets[packet_idx]);

		// Move to next packet
		group.current_packet_index++;

		// Check if there are more packets in this group
		if (group.current_packet_index < group.packets.size()) {
			// Calculate when the next packet should be injected
			DWORD next_packet_interval = 0;
			if (group.current_packet_index < config.packet_intervals_ms.size()) {
				next_packet_interval = config.packet_intervals_ms[group.current_packet_index];
			}
			group.next_packet_time_ms = new_timestamp + next_packet_interval;

			// Re-queue the group for the next packet
			EnterCriticalSection(&injection_queue_cs);
			packet_queues[queue_name].push(group);
			LeaveCriticalSection(&injection_queue_cs);

			if (queue_name == "DIRECT" || call_count % 25 == 1) {
				std::wstring qname_w(queue_name.begin(), queue_name.end());
				DEBUGLOG(L"[INJECT-REQUEUE] Re-queued '" + qname_w +
					L"' for packet " + std::to_wstring(group.current_packet_index + 1) +
					L" in " + std::to_wstring(next_packet_interval) + L"ms");
			}
		} else {
			// Group is complete, update last injection time for inter-group delay
			EnterCriticalSection(&injection_queue_cs);
			queue_configs[queue_name].last_injection_time_ms = new_timestamp;
			LeaveCriticalSection(&injection_queue_cs);
		}
	}
}

decltype(CreateWindowExA) *_CreateWindowExA = NULL;
HWND WINAPI CreateWindowExA_Hook(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
	if (lpClassName && strcmp(lpClassName, "MapleStoryClass") == 0) {
		HWND hRet = _CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
		if (!bInjectorCallback) {
			bInjectorCallback = true;
			SetTimer(hRet, 1337, 10, PacketInjector);  // Increased from 50ms to 10ms (100Hz instead of 20Hz)
			DEBUG(L"MAIN THREAD OK 2");
			DEBUGLOG(L"PacketInjector timer callback installed (via CreateWindowExA hook)");
		}
		return hRet;
	}
	return _CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

BOOL CALLBACK SearchMaple(HWND hwnd, LPARAM lParam) {
	DWORD pid = 0;
	WCHAR wcClassName[256] = { 0 };
	if (GetWindowThreadProcessId(hwnd, &pid)) {
		if (pid == GetCurrentProcessId()) {
			if (GetClassNameW(hwnd, wcClassName, _countof(wcClassName) - 1)) {
				if (wcscmp(wcClassName, L"MapleStoryClass") == 0) {
					if (!bInjectorCallback) {
						bInjectorCallback = true;
						SetTimer(hwnd, 1337, 10, PacketInjector);  // Increased from 50ms to 10ms (100Hz instead of 20Hz)
						DEBUG(L"MAIN THREAD OK 1");
						DEBUGLOG(L"PacketInjector timer callback installed (via SearchMaple)");
					}
				}
				return FALSE;
			}
		}
	}
	return TRUE;
}

bool SetCallBack() {
	if (bInjectorCallback) {
		return true;
	}

	EnumWindows(SearchMaple, NULL);
	return bInjectorCallback;
}

bool SetCallBackThread() {
	while (bInjectorCallback == false) {
		SetCallBack();
		Sleep(1000);
	}

	return true;
}

bool SetBackdoor() {
	HANDLE hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)SetCallBackThread, NULL, NULL, NULL);
	if (hThread) {
		CloseHandle(hThread);
	}
	SHook(CreateWindowExA);
	return true;
}

bool RunPacketSender() {
	SetBackdoor();
	return true;
}
