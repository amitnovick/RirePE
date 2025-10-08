// TCP wrapper implementation - separate file to avoid winsock header conflicts
// This file must be compiled separately and include SimpleTCP.h BEFORE Windows.h

#include"../Share/Simple/SimpleTCP.h"
#include"../Share/Simple/Simple.h"
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
	// Store the current client connection
	EnterCriticalSection(&tcp_client_cs);
	current_client = &client;
	LeaveCriticalSection(&tcp_client_cs);

	// Keep connection alive - the worker thread will use this connection
	// to send/recv data. Connection stays open until client disconnects.
	while (true) {
		Sleep(100); // Keep thread alive
	}

	// Client disconnected
	EnterCriticalSection(&tcp_client_cs);
	current_client = NULL;
	LeaveCriticalSection(&tcp_client_cs);

	return true;
}

bool StartTCPClient() {
	InitTracking();

	if (!tcp_cs_initialized) {
		InitializeCriticalSection(&tcp_client_cs);
		tcp_cs_initialized = true;
	}

	// Create TCP server (note: g_TCPPort is used, g_TCPHost is ignored for server)
	ts = new TCPServer(g_TCPPort);
	ts->SetCommunicate(TCPCommunicate);
	return ts->Run();
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
	EnterCriticalSection(&tcp_client_cs);
	bool success = false;
	if (current_client) {
		success = current_client->Send(bData, uLength);
	}
	LeaveCriticalSection(&tcp_client_cs);
	return success;
}

bool RecvPacketDataTCP(std::vector<BYTE> &vData) {
	EnterCriticalSection(&tcp_client_cs);
	bool success = false;
	if (current_client) {
		success = current_client->Recv(vData);
	}
	LeaveCriticalSection(&tcp_client_cs);
	return success;
}
