#ifndef __SIMPLETCP_H__
#define __SIMPLETCP_H__

// Must include winsock2 BEFORE windows.h to avoid conflicts with old winsock.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <vector>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define TCP_MESSAGE_MAGIC 0xA11CE

#pragma pack(push, 1)
typedef struct {
	DWORD magic;
	DWORD length;
	BYTE data[1];
} TCPMessage;
#pragma pack(pop)

// TCP Server Thread - handles a single client connection
class TCPServerThread {
private:
	SOCKET client_socket;
	bool (*communicate)(TCPServerThread&);

public:
	TCPServerThread(SOCKET sock, bool (*function)(TCPServerThread&));
	~TCPServerThread();

	bool Run();
	bool Send(BYTE *bData, ULONG_PTR uLength);
	bool Recv(std::vector<BYTE> &vData);
	bool Send(std::wstring wText);
	bool Recv(std::wstring &wText);
};

// TCP Server - listens for connections on a port
class TCPServer {
private:
	int port;
	SOCKET listen_socket;
	static bool (*communicate)(TCPServerThread&);

	static DWORD WINAPI ClientThreadProc(LPVOID param);

public:
	TCPServer(int nPort);
	~TCPServer();

	bool SetCommunicate(bool (*function)(TCPServerThread&));
	bool Run();
	void Stop();
};

// TCP Client - connects to a TCP server
class TCPClient {
private:
	std::string host;
	int port;
	SOCKET client_socket;

public:
	TCPClient(std::string sHost, int nPort);
	~TCPClient();

	bool Run();
	bool Send(BYTE *bData, ULONG_PTR uLength);
	bool Recv(std::vector<BYTE> &vData);
	bool Send(std::wstring wText);
	bool Recv(std::wstring &wText);
};

#endif
