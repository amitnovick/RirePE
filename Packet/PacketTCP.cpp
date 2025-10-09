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

// Communication callback for TCP server - handles each client connection
bool TCPCommunicate(TCPServerThread &client) {
	DEBUGLOG(L"[TCP] Client connected to TCP server");

	// Store the current client connection
	EnterCriticalSection(&tcp_client_cs);
	current_client = &client;
	LeaveCriticalSection(&tcp_client_cs);
	DEBUGLOG(L"[TCP] Client pointer stored, ready for communication");

	// Keep connection alive - packets will be sent/received by PacketQueue worker thread
	// This thread just needs to keep the client object alive and detect disconnection
	// We do this by waiting for a dummy receive that will fail when client disconnects
	std::vector<BYTE> dummy;
	while (true) {
		// Try to receive with a timeout (this will block until data or disconnect)
		// Since Python client only sends responses (single bytes), we don't expect
		// unsolicited data. If we get anything or connection closes, we exit.
		Sleep(1000); // Check periodically if we should keep running

		// Simple liveness check - could also implement a proper heartbeat
		// For now, just keep the connection alive
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
	// Get client pointer atomically
	EnterCriticalSection(&tcp_client_cs);
	TCPServerThread *client = current_client;
	LeaveCriticalSection(&tcp_client_cs);

	// Send outside critical section to avoid blocking
	if (client) {
		bool result = client->Send(bData, uLength);
		if (!result) {
			DEBUGLOG(L"[TCP] SendPacketDataTCP failed - client send error");
		}
		return result;
	} else {
		static bool logged_no_client = false;
		if (!logged_no_client) {
			DEBUGLOG(L"[TCP] SendPacketDataTCP - no TCP client connected");
			logged_no_client = true;
		}
	}
	return false;
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
