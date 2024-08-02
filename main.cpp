#include<iostream>
#include<WinSock2.h>

extern "C" {
#include"rtmp.h"
#include"log.h"
}
#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib,"ws2_32.lib")

bool openFLV(const char* url, FILE** fp) {
	errno_t err = fopen_s(fp,url, "rb");
	if (err != 0)return false;

	fseek(*fp, 9, SEEK_SET);
	fseek(*fp, 4, SEEK_CUR);

	return true;
}

int read_data(FILE* fp, RTMPPacket** pkt) {
	char tag[11] = "";
	if (fread(tag, 1, 11, fp) != 11)return 0;

	uint32_t size = (tag[1] << 16 & 0xFF0000) | ((tag[2] << 8) & 0xFF00) |
		(tag[3] & 0xFF);
	// 跳过script数据
	if (tag[0] != 0x09 && tag[0] != 0x08) {
		fseek(fp, size + 4, SEEK_CUR);
		return 2;
	}
	
	int ret = fread((*pkt)->m_body, 1, size,fp);
	if (ret != size)return 0;

	(*pkt)->m_headerType = RTMP_PACKET_SIZE_LARGE;
	(*pkt)->m_nBodySize = size;
	uint32_t ti = tag[4] << 16 & 0xFF0000 | (tag[5] << 8 & 0xFF00)
		| (tag[6] & 0xFF);
	(*pkt)->m_nTimeStamp = ti;
	(*pkt)->m_packetType = tag[0];
	static int index = 1;
	std::cout << "type ：" << (int)tag[0] << " 数量 : " << index++ << " 时间戳："<< ti << std::endl;
	fseek(fp, 4, SEEK_CUR);
	return 1;
}

int main(int argc, char* argv[]) {
	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, failed to WSAStartup. Error: %d", __FUNCTION__, WSAGetLastError());
		return FALSE;
	}

	RTMP* rtmp = RTMP_Alloc();
	if (!rtmp)return -1;

	RTMP_Init(rtmp);

	rtmp->Link.timeout = 10;
	RTMP_SetupURL(rtmp, (char*)"rtmp://127.0.0.1:1935/live/test");

	RTMP_EnableWrite(rtmp);

	if (!RTMP_Connect(rtmp, NULL))
	{
		std::cerr << "Failed to connect RTMP." << std::endl;
		RTMP_Free(rtmp);
		return -1;
	}

	if (!RTMP_ConnectStream(rtmp, 0))
	{
		std::cerr << "Failed to connect stream." << std::endl;
		RTMP_Close(rtmp);
		RTMP_Free(rtmp);
		return -1;
	}

	RTMPPacket* pkt = new RTMPPacket;
	RTMPPacket_Alloc(pkt, 1024 * 1024);

	RTMPPacket_Reset(pkt);

	pkt->m_hasAbsTimestamp = 0;
	pkt->m_nChannel = 0x04;
	pkt->m_nInfoField2 = rtmp->m_stream_id;

	uint32_t ts = 0;

	FILE* fp = nullptr;
	if (!openFLV("video.flv", &fp))return -1;


	while (true) {
		ret = read_data(fp, &pkt);
		if (ret == 0)break;
		else if (ret == 2)continue;

		if (!RTMP_IsConnected(rtmp)) break;

		uint32_t diff = pkt->m_nTimeStamp - ts;
		if (diff < 0)diff = 0;

		Sleep(1);

		RTMP_SendPacket(rtmp, pkt, 0);

		ts = pkt->m_nTimeStamp;
	}
	std::cout << "push OK...." << std::endl;
	// 关闭RTMP连接
	RTMP_Close(rtmp);
	RTMP_Free(rtmp);
	return 0;
}