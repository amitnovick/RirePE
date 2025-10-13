#include"../Share/Simple/Simple.h"
#include"../Share/Hook/SimpleHook.h"
#include"../Packet/PacketHook.h"
#include"PacketDefs.h"
#include"../Share/Simple/DebugLog.h"
#include <queue>

bool bInjectorCallback = false;

// Replace single-slot injection with a proper queue
std::queue<std::vector<BYTE>> injection_queue;
CRITICAL_SECTION injection_queue_cs;
bool injection_queue_initialized = false;

VOID CALLBACK PacketInjector(HWND, UINT, UINT_PTR, DWORD) {
	// Initialize critical section on first call
	if (!injection_queue_initialized) {
		InitializeCriticalSection(&injection_queue_cs);
		injection_queue_initialized = true;
	}

	// Process up to 5 packets per timer tick (50ms) to improve throughput
	// This gives us 100 packets/second max (vs 20/second with single packet processing)
	const int MAX_PACKETS_PER_TICK = 5;
	int processed = 0;

	while (processed < MAX_PACKETS_PER_TICK) {
		// Check if there's a packet to inject
		std::vector<BYTE> data;
		EnterCriticalSection(&injection_queue_cs);
		if (injection_queue.empty()) {
			LeaveCriticalSection(&injection_queue_cs);
			break;
		}
		data = injection_queue.front();
		injection_queue.pop();
		size_t queue_size = injection_queue.size();
		LeaveCriticalSection(&injection_queue_cs);

		processed++;
		DEBUGLOG(L"PacketInjector: Processing injection request #" + std::to_wstring(processed) +
			L" (remaining in queue: " + std::to_wstring(queue_size) + L")...");

		PacketEditorMessage *pcm = (PacketEditorMessage *)&data[0];
		DEBUGLOG(L"PacketInjector: Message type = " + std::to_wstring(pcm->header) +
			L" (SENDPACKET=" + std::to_wstring(SENDPACKET) + L", RECVPACKET=" + std::to_wstring(RECVPACKET) + L")");

		if (pcm->header == SENDPACKET) {
			DEBUGLOG(L"PacketInjector: SENDPACKET requested");
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
			DEBUGLOGHEX(L"PacketInjector: Packet length", pcm->Binary.length);
			DEBUGLOGHEX(L"PacketInjector: Packet header", *(WORD *)&pcm->Binary.packet[0]);

			extern void (*_EnterSendPacket_Original)(OutPacket *op);

			if (_EnterSendPacket_Original == NULL) {
				DEBUGLOG(L"PacketInjector: CRITICAL ERROR - _EnterSendPacket_Original is NULL!");
				DEBUGLOG(L"PacketInjector: This means EnterSendPacket was not found during AOB scan");
				DEBUGLOG(L"PacketInjector: The SendPacket button cannot work without this!");
				continue; // Skip this packet and try next one
			}

			DEBUGLOGHEX(L"PacketInjector: Using EnterSendPacket_Original", (ULONG_PTR)_EnterSendPacket_Original);
			DEBUGLOG(L"PacketInjector: Calling COutPacket_Hook...");

			// Initialize OutPacket structure with header using COutPacket
			WORD wHeader = *(WORD *)&pcm->Binary.packet[0];
			OutPacket p;
			memset(&p, 0, sizeof(p));
			COutPacket_Hook(&p, 0, wHeader);

			DEBUGLOG(L"PacketInjector: COutPacket_Hook completed");

			// Set up the packet data
			p.packet = &pcm->Binary.packet[0];
			p.encoded = pcm->Binary.length;

			DEBUGLOGHEX(L"PacketInjector: Packet pointer", (ULONG_PTR)p.packet);
			DEBUGLOGHEX(L"PacketInjector: Packet encoded length", p.encoded);

			DEBUGLOG(L"PacketInjector: Calling _EnterSendPacket_Original...");
			_EnterSendPacket_Original(&p);
			DEBUGLOG(L"PacketInjector: Packet sent successfully!");
#endif
		}
		else if (pcm->header == RECVPACKET) {
			DEBUGLOG(L"PacketInjector: RECVPACKET requested");
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
			DEBUGLOG(L"PacketInjector: RECVPACKET injection completed");
		}
		else {
			DEBUGLOG(L"PacketInjector: WARNING - Unknown message type: " + std::to_wstring(pcm->header));
		}
	} // End while loop
}

decltype(CreateWindowExA) *_CreateWindowExA = NULL;
HWND WINAPI CreateWindowExA_Hook(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
	if (lpClassName && strcmp(lpClassName, "MapleStoryClass") == 0) {
		HWND hRet = _CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
		if (!bInjectorCallback) {
			bInjectorCallback = true;
			SetTimer(hRet, 1337, 50, PacketInjector);
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
						SetTimer(hwnd, 1337, 50, PacketInjector);
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

// Pipe server removed - packet injection now handled via TCP in PacketTCP.cpp

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
	// Set up timer callback for packet injection
	// Packet reception now handled via TCP instead of pipe server
	SetBackdoor();
	return true;
}
