#include "SimpleTCP.h"
#include <iostream>

// ============================================================================
// Helper function to initialize Winsock (call once)
// ============================================================================
static bool g_WinsockInitialized = false;

bool InitializeWinsock() {
	if (g_WinsockInitialized) {
		return true;
	}

	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		return false;
	}

	g_WinsockInitialized = true;
	return true;
}

// ============================================================================
// TCPServerThread Implementation
// ============================================================================

TCPServerThread::TCPServerThread(SOCKET sock, bool (*function)(TCPServerThread&)) {
	client_socket = sock;
	communicate = function;
}

TCPServerThread::~TCPServerThread() {
	if (client_socket != INVALID_SOCKET) {
		closesocket(client_socket);
		client_socket = INVALID_SOCKET;
	}
}

bool TCPServerThread::Run() {
	if (communicate) {
		return communicate(*this);
	}
	return false;
}

bool TCPServerThread::Send(BYTE *bData, ULONG_PTR uLength) {
	if (client_socket == INVALID_SOCKET || !bData || uLength == 0) {
		return false;
	}

	// Allocate message buffer
	DWORD total_size = sizeof(DWORD) * 2 + (DWORD)uLength;
	BYTE *buffer = new BYTE[total_size];
	if (!buffer) {
		return false;
	}

	// Build message with magic and length
	TCPMessage *msg = (TCPMessage*)buffer;
	msg->magic = TCP_MESSAGE_MAGIC;
	msg->length = (DWORD)uLength;
	memcpy(msg->data, bData, uLength);

	// Send the entire message
	int sent = send(client_socket, (char*)buffer, total_size, 0);
	delete[] buffer;

	return (sent == total_size);
}

bool TCPServerThread::Recv(std::vector<BYTE> &vData) {
	if (client_socket == INVALID_SOCKET) {
		return false;
	}

	// Receive magic and length (header)
	DWORD magic = 0;
	DWORD length = 0;

	int received = recv(client_socket, (char*)&magic, sizeof(DWORD), MSG_WAITALL);
	if (received != sizeof(DWORD) || magic != TCP_MESSAGE_MAGIC) {
		return false;
	}

	received = recv(client_socket, (char*)&length, sizeof(DWORD), MSG_WAITALL);
	if (received != sizeof(DWORD) || length == 0 || length > 1024 * 1024) {
		return false;
	}

	// Allocate buffer and receive data
	vData.resize(length);
	received = recv(client_socket, (char*)&vData[0], length, MSG_WAITALL);
	if (received != (int)length) {
		vData.clear();
		return false;
	}

	return true;
}

bool TCPServerThread::Send(std::wstring wText) {
	std::string sText(wText.begin(), wText.end());
	return Send((BYTE*)sText.c_str(), sText.length());
}

bool TCPServerThread::Recv(std::wstring &wText) {
	std::vector<BYTE> vData;
	if (!Recv(vData)) {
		return false;
	}

	std::string sText((char*)&vData[0], vData.size());
	wText = std::wstring(sText.begin(), sText.end());
	return true;
}

// ============================================================================
// TCPServer Implementation
// ============================================================================

bool (*TCPServer::communicate)(TCPServerThread&) = NULL;

TCPServer::TCPServer(int nPort) {
	port = nPort;
	listen_socket = INVALID_SOCKET;
}

TCPServer::~TCPServer() {
	Stop();
}

void TCPServer::Stop() {
	if (listen_socket != INVALID_SOCKET) {
		closesocket(listen_socket);
		listen_socket = INVALID_SOCKET;
	}
}

bool TCPServer::SetCommunicate(bool (*function)(TCPServerThread&)) {
	communicate = function;
	return true;
}

DWORD WINAPI TCPServer::ClientThreadProc(LPVOID param) {
	SOCKET client_socket = (SOCKET)(ULONG_PTR)param;
	TCPServerThread thread(client_socket, communicate);
	thread.Run();
	return 0;
}

bool TCPServer::Run() {
	if (!InitializeWinsock()) {
		return false;
	}

	// Create listening socket
	listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == INVALID_SOCKET) {
		return false;
	}

	// Allow socket reuse
	int optval = 1;
	setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

	// Bind to all interfaces
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((u_short)port);

	if (bind(listen_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		closesocket(listen_socket);
		listen_socket = INVALID_SOCKET;
		return false;
	}

	// Start listening
	if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
		closesocket(listen_socket);
		listen_socket = INVALID_SOCKET;
		return false;
	}

	// Accept connections in background thread
	CreateThread(NULL, 0, [](LPVOID param) -> DWORD {
		TCPServer* server = (TCPServer*)param;
		while (server->listen_socket != INVALID_SOCKET) {
			SOCKET client_socket = accept(server->listen_socket, NULL, NULL);
			if (client_socket != INVALID_SOCKET) {
				// Spawn thread for each client
				CreateThread(NULL, 0, ClientThreadProc, (LPVOID)(ULONG_PTR)client_socket, 0, NULL);
			}
		}
		return 0;
	}, this, 0, NULL);

	return true;
}

// ============================================================================
// TCPClient Implementation
// ============================================================================

TCPClient::TCPClient(std::string sHost, int nPort) {
	host = sHost;
	port = nPort;
	client_socket = INVALID_SOCKET;
}

TCPClient::~TCPClient() {
	if (client_socket != INVALID_SOCKET) {
		closesocket(client_socket);
		client_socket = INVALID_SOCKET;
	}
}

bool TCPClient::Run() {
	if (!InitializeWinsock()) {
		return false;
	}

	// Create socket
	client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client_socket == INVALID_SOCKET) {
		return false;
	}

	// Set up server address
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)port);

	// Convert hostname to IP
	if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
		// Try DNS lookup
		struct addrinfo hints = {0}, *result = NULL;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		if (getaddrinfo(host.c_str(), NULL, &hints, &result) != 0 || !result) {
			closesocket(client_socket);
			client_socket = INVALID_SOCKET;
			return false;
		}

		addr.sin_addr = ((sockaddr_in*)result->ai_addr)->sin_addr;
		freeaddrinfo(result);
	}

	// Connect to server
	if (connect(client_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		closesocket(client_socket);
		client_socket = INVALID_SOCKET;
		return false;
	}

	return true;
}

bool TCPClient::Send(BYTE *bData, ULONG_PTR uLength) {
	if (client_socket == INVALID_SOCKET || !bData || uLength == 0) {
		return false;
	}

	// Allocate message buffer
	DWORD total_size = sizeof(DWORD) * 2 + (DWORD)uLength;
	BYTE *buffer = new BYTE[total_size];
	if (!buffer) {
		return false;
	}

	// Build message with magic and length
	TCPMessage *msg = (TCPMessage*)buffer;
	msg->magic = TCP_MESSAGE_MAGIC;
	msg->length = (DWORD)uLength;
	memcpy(msg->data, bData, uLength);

	// Send the entire message
	int sent = send(client_socket, (char*)buffer, total_size, 0);
	delete[] buffer;

	return (sent == total_size);
}

bool TCPClient::Recv(std::vector<BYTE> &vData) {
	if (client_socket == INVALID_SOCKET) {
		return false;
	}

	// Receive magic and length (header)
	DWORD magic = 0;
	DWORD length = 0;

	int received = recv(client_socket, (char*)&magic, sizeof(DWORD), MSG_WAITALL);
	if (received != sizeof(DWORD) || magic != TCP_MESSAGE_MAGIC) {
		return false;
	}

	received = recv(client_socket, (char*)&length, sizeof(DWORD), MSG_WAITALL);
	if (received != sizeof(DWORD) || length == 0 || length > 1024 * 1024) {
		return false;
	}

	// Allocate buffer and receive data
	vData.resize(length);
	received = recv(client_socket, (char*)&vData[0], length, MSG_WAITALL);
	if (received != (int)length) {
		vData.clear();
		return false;
	}

	return true;
}

bool TCPClient::Send(std::wstring wText) {
	std::string sText(wText.begin(), wText.end());
	return Send((BYTE*)sText.c_str(), sText.length());
}

bool TCPClient::Recv(std::wstring &wText) {
	std::vector<BYTE> vData;
	if (!Recv(vData)) {
		return false;
	}

	std::string sText((char*)&vData[0], vData.size());
	wText = std::wstring(sText.begin(), sText.end());
	return true;
}
