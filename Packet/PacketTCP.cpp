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
extern std::map<WORD, std::string> opcode_to_queue_map;
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
struct QueueConfig {
	std::string queue_name;
	DWORD injection_interval_ms;
	DWORD last_injection_time_ms;
	std::vector<WORD> packet_opcodes;
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
		if (data.size() >= sizeof(PacketEditorMessage)) {
			PacketEditorMessage* pcm = (PacketEditorMessage*)&data[0];

			if (pcm->header == SENDPACKET || pcm->header == RECVPACKET) {
				DEBUGLOG(L"[TCP] Packet injection request: " +
					std::wstring(pcm->header == SENDPACKET ? L"SENDPACKET" : L"RECVPACKET"));

				// Log first few bytes of packet
				std::wstring packet_preview = L"[TCP] Packet data (first 16 bytes): ";
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

				// Determine packet opcode
				WORD opcode = 0;
				if (pcm->Binary.length >= 2) {
					opcode = *(WORD*)&pcm->Binary.packet[0];
				}

				// Look up which queue this opcode belongs to
				EnterCriticalSection(&injection_queue_cs);

				auto opcode_map_it = opcode_to_queue_map.find(opcode);

				// BACKWARD COMPATIBILITY: If no queue is registered for this opcode,
				// route it to the legacy "DIRECT" queue for immediate injection
				if (opcode_map_it == opcode_to_queue_map.end()) {
					// Use the legacy direct injection queue
					std::string legacy_queue_name = "DIRECT";

					// Ensure legacy queue exists (create on first use)
					auto legacy_config_it = queue_configs.find(legacy_queue_name);
					if (legacy_config_it == queue_configs.end()) {
						// Create legacy queue configuration on-demand using QueueConfigMessage
						// Use a dummy opcode (0xFFFF) since legacy queue accepts ALL opcodes
						QueueConfigMessage legacy_msg;
						memset(&legacy_msg, 0, sizeof(legacy_msg));

						// Set queue name
						strncpy_s(legacy_msg.queue_name, MAX_QUEUE_NAME_LENGTH, legacy_queue_name.c_str(), _TRUNCATE);

						// No delay for legacy packets (immediate injection)
						legacy_msg.injection_interval_ms = 0;

						// Single packet in group
						legacy_msg.packet_count = 1;
						legacy_msg.packet_opcodes[0] = 0xFFFF;  // Dummy opcode (not used for routing)

						// No timestamp updates (legacy packets have timestamps pre-filled)
						legacy_msg.timestamp_configs[0].needs_timestamp_update = 0;
						legacy_msg.timestamp_configs[0].timestamp_offset_count = 0;

						// Register the legacy queue using the standard registration function
						RegisterQueue(legacy_msg);

						DEBUGLOG(L"[TCP-LEGACY] Created on-demand legacy queue for backward compatibility");
					}

					// Create single-packet group and add directly to queue
					// Note: We do NOT add to opcode_to_queue_map because legacy queue
					// accepts any opcode that isn't already registered
					MultiPacketGroup group;
					group.packets.push_back(data);
					group.queued_time_ms = GetTickCount();
					packet_queues[legacy_queue_name].push(group);

					LeaveCriticalSection(&injection_queue_cs);

					DEBUGLOG(L"[TCP-LEGACY] Packet routed to DIRECT queue (opcode=0x" +
						std::to_wstring(opcode) + L") - v1 compatibility mode");
					continue;
				}

				std::string queue_name = opcode_map_it->second;

				// Get the queue configuration to determine how many packets are expected
				auto config_it = queue_configs.find(queue_name);
				if (config_it == queue_configs.end()) {
					LeaveCriticalSection(&injection_queue_cs);
					DEBUGLOG(L"[TCP] ERROR: Queue config not found for queue: " +
						std::wstring(queue_name.begin(), queue_name.end()));
					continue;
				}

				QueueConfig& config = config_it->second;
				size_t expected_packet_count = config.packet_opcodes.size();

				// Get or create incomplete group for this queue
				IncompleteGroup& incomplete = incomplete_groups[queue_name];

				// If this is the first packet in the group, initialize timestamp
				if (incomplete.packets.empty()) {
					incomplete.start_time_ms = GetTickCount();
				}

				// Add packet to incomplete group
				incomplete.packets.push_back(data);

				std::wstring queue_name_w(queue_name.begin(), queue_name.end());
				DEBUGLOG(L"[TCP] Added packet " + std::to_wstring(incomplete.packets.size()) +
					L"/" + std::to_wstring(expected_packet_count) + L" to queue '" +
					queue_name_w + L"' (opcode=0x" + std::to_wstring(opcode) + L")");

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
			} else {
				DEBUGLOG(L"[TCP] Ignoring message type: " + std::to_wstring(pcm->header));
			}
		} else {
			DEBUGLOG(L"[TCP] Received data too small to be PacketEditorMessage (got " +
				std::to_wstring(data.size()) + L" bytes, need at least " +
				std::to_wstring(sizeof(PacketEditorMessage)) + L" bytes)");
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
