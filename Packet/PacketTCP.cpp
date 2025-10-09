// TCP wrapper implementation - separate file to avoid winsock header conflicts
// This file must be compiled separately and include SimpleTCP.h BEFORE Windows.h

#include"../Share/Simple/SimpleTCP.h"
#include"../Share/Simple/Simple.h"
#include"../Share/Simple/DebugLog.h"
#include"PacketLogging.h"
#include<vector>

// TCP server instance and connected client thread
TCPServer *ts = NULL;
TCPServerThread *current_client = NULL;
CRITICAL_SECTION tcp_client_cs;
bool tcp_cs_initialized = false;

// TCP configuration (defined in PacketLogging.cpp)
extern bool g_UseTCP;
extern std::string g_TCPHost;
extern int g_TCPPort;

// Initialize tracking (defined in PacketLogging.cpp)
extern void InitTracking();

// External references to packet injection infrastructure (from PacketSender.cpp)
extern bool bToBeInject;
extern std::vector<BYTE> global_data;

// Communication callback for TCP server - handles each client connection
bool TCPCommunicate(TCPServerThread &client) {
	DEBUGLOG(L"[TCP] Client connected to TCP server");

	// Store the current client connection
	EnterCriticalSection(&tcp_client_cs);
	current_client = &client;
	LeaveCriticalSection(&tcp_client_cs);
	DEBUGLOG(L"[TCP] Client pointer stored, ready for communication");

	// Process incoming commands from TCP client (similar to CommunicateThread for pipes)
	std::vector<BYTE> data;
	while (true) {
		// Receive framed message from client
		if (!client.Recv(data)) {
			// Connection closed or error
			DEBUGLOG(L"[TCP] Client disconnected or receive failed");
			break;
		}

		DEBUGLOG(L"[TCP] Received " + std::to_wstring(data.size()) + L" bytes from client");

		// Check if this is a packet injection command (PacketEditorMessage)
		if (data.size() >= sizeof(PacketEditorMessage)) {
			PacketEditorMessage* pcm = (PacketEditorMessage*)&data[0];

			// Only inject SENDPACKET and RECVPACKET commands
			if (pcm->header == SENDPACKET || pcm->header == RECVPACKET) {
				DEBUGLOG(L"[TCP] Packet injection request: " +
					std::wstring(pcm->header == SENDPACKET ? L"SENDPACKET" : L"RECVPACKET"));

				// Use same injection mechanism as pipe (via timer callback)
				if (!bToBeInject) {
					global_data.clear();
					global_data = data;
					bToBeInject = true;
					DEBUGLOG(L"[TCP] Packet queued for injection");
				} else {
					DEBUGLOG(L"[TCP] Injection already pending, dropping packet");
				}
			} else {
				DEBUGLOG(L"[TCP] Ignoring non-injection message type: " + std::to_wstring(pcm->header));
			}
		} else {
			DEBUGLOG(L"[TCP] Received data too small to be PacketEditorMessage");
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
