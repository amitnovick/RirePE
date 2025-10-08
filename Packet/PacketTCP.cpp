// TCP wrapper implementation - separate file to avoid winsock header conflicts
// This file must be compiled separately and include SimpleTCP.h BEFORE Windows.h

#include"../Share/Simple/SimpleTCP.h"
#include"../Share/Simple/Simple.h"
#include"PacketLogging.h"
#include<vector>

// TCP client instance (defined in PacketLogging.cpp)
TCPClient *tc = NULL;

// TCP configuration (defined in PacketLogging.cpp)
extern bool g_UseTCP;
extern std::string g_TCPHost;
extern int g_TCPPort;

// Initialize tracking (defined in PacketLogging.cpp)
extern void InitTracking();

bool StartTCPClient() {
	InitTracking();
	tc = new TCPClient(g_TCPHost, g_TCPPort);
	return tc->Run();
}

bool RestartTCPClient() {
	if (tc) {
		delete tc;
		tc = NULL;
	}
	return StartTCPClient();
}

// Abstract send/recv functions (called from PacketQueue)
bool SendPacketDataTCP(BYTE *bData, ULONG_PTR uLength) {
	if (tc) {
		return tc->Send(bData, uLength);
	}
	return false;
}

bool RecvPacketDataTCP(std::vector<BYTE> &vData) {
	if (tc) {
		return tc->Recv(vData);
	}
	return false;
}
