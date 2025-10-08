#ifndef __PACKET_LOGGING_H__
#define __PACKET_LOGGING_H__

#include"../RirePE/RirePE.h"
#include"../Packet/PacketHook.h"
#include"../Share/Simple/Simple.h"
#include<vector>
#include<unordered_map>

typedef struct {
	DWORD id; // パケット識別子
	ULONGLONG addr; // リターンアドレス
	MessageHeader fmt; // フォーマットの種類
	DWORD pos; // 場所
	DWORD size; // データの長さ
	BYTE *data;
	ULONG_PTR tracking;
} PacketExtraInformation;

bool InPacketLogging(MessageHeader type, InPacket *ip, void *retAddr);
bool OutPacketLogging(MessageHeader type, OutPacket *op, void *retAddr);

DWORD CountUpPacketID(DWORD &id);

extern DWORD packet_id_out;
extern DWORD packet_id_in;

void ClearQueue(OutPacket *op);

// Inline for performance - called on every encode operation
inline void AddQueue(PacketExtraInformation &pxi);
void AddExtra(PacketExtraInformation &pxi);
void AddSendPacket(OutPacket *op, ULONG_PTR addr, bool &bBlock);
void AddRecvPacket(InPacket *ip, ULONG_PTR addr, bool &bBlock);

// Global settings
extern bool g_EnableBlocking;

extern PipeClient *pc;
extern CRITICAL_SECTION cs;
bool StartPipeClient();
bool RestartPipeClient();


#endif