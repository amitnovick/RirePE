// TCP wrapper implementation - Multi-packet queue support
// This file must be compiled separately and include SimpleTCP.h BEFORE Windows.h

#include"../Share/Simple/SimpleTCP.h"
#include"../Share/Simple/Simple.h"
#include"../Share/Simple/DebugLog.h"
#include"PacketLogging.h"
#include"PacketDefs.h"
#include <vector>
#include <queue>
#include <map>
#include <string>

// TCP server instance and connected client thread
TCPServer *ts = NULL;
TCPServerThread *current_client = NULL;
CRITICAL_SECTION tcp_client_cs;
bool tcp_cs_initialized = false;

// TCP configuration (defined in PacketLogging.cpp)
extern std::string g_TCPHost;
extern int g_TCPPort;

// Initialize tracking (defined in PacketLogging.cpp)
extern void InitTracking();

// Multi-packet group structure
struct MultiPacketGroup {
	std::vector<std::vector<BYTE>> packets;
	DWORD queued_time_ms;
};

// External references to packet injection infrastructure (from PacketSender.cpp)
extern std::map<std::string, std::queue<MultiPacketGroup>> packet_queues;
extern CRITICAL_SECTION injection_queue_cs;
extern bool injection_queue_initialized;

// Temporary storage for incomplete multi-packet groups being assembled
struct IncompleteGroup {
	std::vector<std::vector<BYTE>> packets;
	DWORD start_time_ms;
};
extern std::map<std::string, IncompleteGroup> incomplete_groups;

// Queue management functions (from PacketSender.cpp)
extern bool RegisterQueue(const QueueConfigMessage& config);
extern bool UnregisterQueue(const std::string& queue_name);
extern void ClearAllQueues();

// Queue configuration map (from PacketSender.cpp)
struct TimestampConfig {
	bool needs_update;
	std::vector<DWORD> offsets;
};

struct QueueConfig {
	std::string queue_name;
	DWORD injection_interval_ms;
	DWORD last_injection_time_ms;
	BYTE packet_count;
	std::vector<TimestampConfig> timestamp_configs;
};
extern std::map<std::string, QueueConfig> queue_configs;

// Communication callback for TCP server - handles each client connection
bool TCPCommunicate(TCPServerThread &client) {
	DEBUGLOG(L"[TCP] Client connected to TCP server");

	// Store the current client connection
	EnterCriticalSection(&tcp_client_cs);
	current_client = &client;
	LeaveCriticalSection(&tcp_client_cs);
	DEBUGLOG(L"[TCP] Client pointer stored, ready for communication");

	// Process incoming commands from TCP client
	// Clients can send:
	// 1. REGISTER_QUEUE messages to configure injection queues
	// 2. SENDPACKET/RECVPACKET messages to inject packets (grouped by queue)

	std::vector<BYTE> data;
	while (true) {
		// Receive framed message from client
		if (!client.Recv(data)) {
			// Connection closed or error
			DEBUGLOG(L"[TCP] Client disconnected or receive failed");
			break;
		}

		DEBUGLOG(L"[TCP] Received " + std::to_wstring(data.size()) + L" bytes from client");

		// Parse message header to determine message type
		if (data.size() < sizeof(DWORD)) {
			DEBUGLOG(L"[TCP] Message too small to parse header");
			continue;
		}

		// Read message type (first DWORD)
		MessageHeader msg_type = *(MessageHeader*)&data[0];

		DEBUGLOG(L"[TCP] Message type: " + std::to_wstring(msg_type));

		// Handle REGISTER_QUEUE messages
		if (msg_type == REGISTER_QUEUE) {
			if (data.size() < sizeof(MessageHeader) + sizeof(QueueConfigMessage)) {
				DEBUGLOG(L"[TCP] REGISTER_QUEUE message too small (got " +
					std::to_wstring(data.size()) + L" bytes, need " +
					std::to_wstring(sizeof(MessageHeader) + sizeof(QueueConfigMessage)) + L" bytes)");
				continue;
			}

			// Extract queue configuration
			QueueConfigMessage* config = (QueueConfigMessage*)&data[sizeof(MessageHeader)];

			// Register the queue
			if (RegisterQueue(*config)) {
				DEBUGLOG(L"[TCP] Successfully registered multi-packet queue via TCP");
			} else {
				DEBUGLOG(L"[TCP] Failed to register queue via TCP");
			}
			continue;
		}

		// Handle UNREGISTER_QUEUE messages
		if (msg_type == UNREGISTER_QUEUE) {
			if (data.size() < sizeof(MessageHeader) + MAX_QUEUE_NAME_LENGTH) {
				DEBUGLOG(L"[TCP] UNREGISTER_QUEUE message too small");
				continue;
			}

			char queue_name_buf[MAX_QUEUE_NAME_LENGTH];
			memcpy(queue_name_buf, &data[sizeof(MessageHeader)], MAX_QUEUE_NAME_LENGTH);
			std::string queue_name(queue_name_buf, strnlen(queue_name_buf, MAX_QUEUE_NAME_LENGTH));

			if (UnregisterQueue(queue_name)) {
				std::wstring queue_name_w(queue_name.begin(), queue_name.end());
				DEBUGLOG(L"[TCP] Successfully unregistered queue: " + queue_name_w);
			} else {
				std::wstring queue_name_w(queue_name.begin(), queue_name.end());
				DEBUGLOG(L"[TCP] Failed to unregister queue: " + queue_name_w);
			}
			continue;
		}

		// Handle CLEAR_QUEUES messages
		if (msg_type == CLEAR_QUEUES) {
			ClearAllQueues();
			DEBUGLOG(L"[TCP] Cleared all queue configurations");
			continue;
		}

		// Handle SENDPACKET/RECVPACKET messages (packet injection)
		// Note: Must check message type to avoid misinterpreting queue commands as packets
		if (msg_type == SENDPACKET || msg_type == RECVPACKET) {
			if (data.size() < sizeof(PacketEditorMessage)) {
				DEBUGLOG(L"[TCP] Received data too small to be PacketEditorMessage (got " +
					std::to_wstring(data.size()) + L" bytes, need at least " +
					std::to_wstring(sizeof(PacketEditorMessage)) + L" bytes)");
				continue;
			}

			PacketEditorMessage* pcm = (PacketEditorMessage*)&data[0];
			DEBUGLOG(L"[TCP] Packet injection request: " +
				std::wstring(pcm->header == SENDPACKET ? L"SENDPACKET" : L"RECVPACKET"));

				// Extract queue name from message
				std::string queue_name(pcm->Binary.queue_name, strnlen(pcm->Binary.queue_name, MAX_QUEUE_NAME_LENGTH));
				std::wstring queue_name_w(queue_name.begin(), queue_name.end());

				// Log first few bytes of packet (offset by queue_name field)
				std::wstring packet_preview = L"[TCP] Queue='" + queue_name_w + L"', Packet data (first 16 bytes): ";
				for (DWORD i = 0; i < min(16, pcm->Binary.length); i++) {
					wchar_t hex[4];
					swprintf_s(hex, L"%02X ", pcm->Binary.packet[i]);
					packet_preview += hex;
				}
				DEBUGLOG(packet_preview);

				// Initialize critical section if needed
				if (!injection_queue_initialized) {
					InitializeCriticalSection(&injection_queue_cs);
					injection_queue_initialized = true;
				}

				// Look up queue configuration
				EnterCriticalSection(&injection_queue_cs);

				auto config_it = queue_configs.find(queue_name);
				if (config_it == queue_configs.end()) {
					LeaveCriticalSection(&injection_queue_cs);
					DEBUGLOG(L"[TCP] ERROR: Queue '" + queue_name_w + L"' not registered!");
					continue;
				}

				QueueConfig& config = config_it->second;
				size_t expected_packet_count = config.packet_count;

				// Get or create incomplete group for this queue
				IncompleteGroup& incomplete = incomplete_groups[queue_name];

				// If this is the first packet in the group, initialize timestamp
				if (incomplete.packets.empty()) {
					incomplete.start_time_ms = GetTickCount();
				}

				// Add packet to incomplete group
				incomplete.packets.push_back(data);

				DEBUGLOG(L"[TCP] Added packet " + std::to_wstring(incomplete.packets.size()) +
					L"/" + std::to_wstring(expected_packet_count) + L" to queue '" + queue_name_w + L"'");

				// Check if we've received all packets for this group
				if (incomplete.packets.size() >= expected_packet_count) {
					// Create complete multi-packet group
					MultiPacketGroup group;
					group.packets = incomplete.packets;
					group.queued_time_ms = incomplete.start_time_ms;

					// Add to packet queue
					packet_queues[queue_name].push(group);
					size_t queue_size = packet_queues[queue_name].size();

					DEBUGLOG(L"[TCP] Complete group added to queue '" + queue_name_w +
						L"' (queue size: " + std::to_wstring(queue_size) + L" group(s))");

					// Clear incomplete group
					incomplete.packets.clear();

					if (queue_size > 10 && queue_size % 10 == 0) {
						DEBUGLOG(L"[TCP] WARNING: Queue '" + queue_name_w +
							L"' depth reached " + std::to_wstring(queue_size) + L" groups!");
					}
				}

				LeaveCriticalSection(&injection_queue_cs);
		}
	}

	// Client disconnected
	DEBUGLOG(L"[TCP] Client disconnected from TCP server");
	EnterCriticalSection(&tcp_client_cs);
	if (current_client == &client) {
		current_client = NULL;
	}
	LeaveCriticalSection(&tcp_client_cs);

	return true;
}

bool StartTCPClient() {
	DEBUGLOG(L"[TCP] StartTCPClient() called");
	InitTracking();

	if (!tcp_cs_initialized) {
		InitializeCriticalSection(&tcp_client_cs);
		tcp_cs_initialized = true;
		DEBUGLOG(L"[TCP] Critical section initialized");
	}

	// Create TCP server (note: g_TCPPort is used, g_TCPHost is ignored for server)
	ts = new TCPServer(g_TCPPort);
	ts->SetCommunicate(TCPCommunicate);
	bool result = ts->Run();

	if (result) {
		DEBUGLOG(L"[TCP] Server started successfully on port " + std::to_wstring(g_TCPPort));
	} else {
		DEBUGLOG(L"[TCP] Server failed to start on port " + std::to_wstring(g_TCPPort));
	}

	return result;
}

bool RestartTCPClient() {
	EnterCriticalSection(&tcp_client_cs);
	current_client = NULL;
	LeaveCriticalSection(&tcp_client_cs);

	if (ts) {
		delete ts;
		ts = NULL;
	}
	return StartTCPClient();
}

// Abstract send/recv functions (called from PacketQueue)
bool SendPacketDataTCP(BYTE *bData, ULONG_PTR uLength) {
	static bool had_client = false;
	static int packet_count = 0;
	packet_count++;

	// Get client pointer atomically
	EnterCriticalSection(&tcp_client_cs);
	TCPServerThread *client = current_client;
	LeaveCriticalSection(&tcp_client_cs);

	// Send outside critical section to avoid blocking
	if (client) {
		if (!had_client) {
			DEBUGLOG(L"[TCP] TCP client is now connected - broadcasting packets");
			had_client = true;
		}
		bool result = client->Send(bData, uLength);
		if (!result) {
			DEBUGLOG(L"[TCP] Send failed - client disconnected?");
			had_client = false;
		}
		if (packet_count <= 5 || packet_count % 100 == 0) {
			DEBUGLOG(L"[TCP PACKET #" + std::to_wstring(packet_count) + L"] Sent to TCP client: " + (result ? L"✓" : L"✗"));
		}
		return result;
	}

	// No client connected - this is not an error, just skip TCP send
	if (had_client) {
		DEBUGLOG(L"[TCP] Client disconnected - TCP broadcasting paused");
		had_client = false;
	}
	return true;
}

bool RecvPacketDataTCP(std::vector<BYTE> &vData) {
	// Get client pointer atomically
	EnterCriticalSection(&tcp_client_cs);
	TCPServerThread *client = current_client;
	LeaveCriticalSection(&tcp_client_cs);

	// Recv outside critical section to avoid blocking/deadlock
	if (client) {
		return client->Recv(vData);
	}
	return false;
}
