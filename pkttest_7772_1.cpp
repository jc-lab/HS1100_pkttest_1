#include <stdio.h>
#include <stdint.h>

#if defined(_WIN32)
#include <winsock2.h>
#elif defined(__linux)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define closesocket close
#endif
#include <string.h>
#include <pthread.h>

#include "JsSimpleSocketReceiver.h"

#include <vector>

void hexOnlyDump(const unsigned char *data, int length)
{
	int i;
	for (i = 0; i < length; i++)
		printf("%02x ", data[i]);
}

void hexDump(const unsigned char *data, int length)
{
	int i;
	int pos = 0;
	for (i = 0; i < length / 16; i++) {
		int j;
		printf("%08x  ", pos);
		hexOnlyDump(&data[pos], 16);
		printf("  ");
		for (j = 0; j < 16; j++) {
			char c = (char)data[pos + j];
			if (isprint(c & 0xFF)) {
				printf("%c", c);
			} else {
				printf("?");
			}
		}
		printf("\n");
		pos += 16;
	}
	if (length % 16) {
		int remain = length % 16;
		int j;
		printf("%08x  ", pos);
		hexOnlyDump(&data[pos], remain);
		for (j = 0; j < (16 - remain); j++)
			printf("   ");
		printf("  ");
		for (j = 0; j < remain; j++) {
			char c = (char)data[pos + j];
			if (isprint(c & 0xFF)) {
				printf("%c", c);
			}
			else {
				printf("?");
			}
		}
		printf("\n");
		pos += remain;
	}
}

class SequansClientSocketReceiver : public JsSimpleSocketReceiver<SOCKET>
{
private:
#pragma pack(push, 1)
	typedef struct _tag_proto_header
	{
		unsigned char sig[2]; // 01 05
		unsigned char length[2]; // Big endian
		unsigned char rev_1; // 0x20 : client->server // 0x28/0x08
		unsigned char flags; // 0x80
		unsigned char dataType[2];
		unsigned char rev_2[8];
	} proto_header_t;
#pragma pack(pop)

	struct ProtoContext {
		const proto_header_t *recvHeader;
		const unsigned char *dataPart;
		uint16_t recvProtoLength;
		uint16_t dataType;
	};

public:
	SequansClientSocketReceiver()
	{
	}
	SequansClientSocketReceiver(SOCKET sock)
		: JsSimpleSocketReceiver(sock)
	{

	}
	virtual ~SequansClientSocketReceiver()
	{

	}

	RecvProcessResult recvProcess(const char *buffer, int recvlen, int *processedsize) override
	{
		ProtoContext protoContext;
		protoContext.recvHeader = (const proto_header_t*)buffer;
		protoContext.dataPart = (const unsigned char*)&buffer[sizeof(proto_header_t)];

		if(recvlen < 16)
			return RECVRESULT_DONE;

		if (protoContext.recvHeader->sig[0] != 0x01 || protoContext.recvHeader->sig[1] != 0x05)
			return RECVRESULT_ERROR;

		protoContext.recvProtoLength = ((((uint16_t)protoContext.recvHeader->length[0]) & 0xFF) << 8) | (((uint16_t)protoContext.recvHeader->length[1]) & 0xFF);
		protoContext.dataType = ((((uint16_t)protoContext.recvHeader->dataType[0]) & 0xFF) << 8) | (((uint16_t)protoContext.recvHeader->dataType[1]) & 0xFF);
		if(recvlen < (16 + protoContext.recvProtoLength))
			return RECVRESULT_DONE;

		printf("DATA Received!\n");
		printf("  Length : %d\n", protoContext.recvProtoLength);
		printf("  rev_1/flags : %02x %02x\n", protoContext.recvHeader->rev_1, protoContext.recvHeader->flags);
		printf("  rev_2 : "); hexOnlyDump(protoContext.recvHeader->rev_2, sizeof(protoContext.recvHeader->rev_2)); printf("\n");
		hexDump(protoContext.dataPart, protoContext.recvProtoLength);
		printf("\n");

		if (protoContext.dataType == 0xffff)
		{
			subproc_ffff(&protoContext);
		}

		*processedsize = 16 + protoContext.recvProtoLength;

		printf("============================================\n");

		return RECVRESULT_RETRY;
	}

	void subproc_ffff(ProtoContext *pProtoContext)
	{
		int pos = 4;
		uint32_t some_1;

		some_1 = ((((uint32_t)pProtoContext->dataPart[0]) & 0xFF) << 24) | ((((uint32_t)pProtoContext->dataPart[1]) & 0xFF) << 16) | ((((uint32_t)pProtoContext->dataPart[2]) & 0xFF) << 8) | (((uint32_t)pProtoContext->dataPart[3]) & 0xFF);
		printf("-> some_1 : %08x[%u]\n", some_1, some_1);

		while (pos < pProtoContext->recvProtoLength)
		{
			const unsigned char *subsig = &pProtoContext->dataPart[pos];
			uint16_t dataidx;
			if ((subsig[0] != 0x80) || (subsig[1] != 0x04)) {
				printf("DATA ERROR\n");
				break;
			}

			dataidx = ((((uint16_t)subsig[2]) & 0xFF) << 8) | (((uint16_t)subsig[3]) & 0xFF);
			printf("DATA element : %u / %s\n", dataidx, (const char*)&subsig[4]);

			pos += 4 + strlen((const char*)&subsig[4]) + 1;
		}

		printf("remain -> %d\n", (pProtoContext->recvProtoLength - pos));

	}
};

void *recvthreadProc(void *param)
{
	SequansClientSocketReceiver *receiver = (SequansClientSocketReceiver*)param;
	receiver->run();
	return NULL;
}

int main(int argc, char *argv[])
{
	WSADATA wsadata;
	::WSAStartup(MAKEWORD(2, 0), &wsadata);

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("192.168.15.1");
	sa.sin_port = htons(7772);
	connect(sock, (struct sockaddr*)&sa, sizeof(sa));

	SequansClientSocketReceiver receiver(sock);
	pthread_t recvthread;
	pthread_create(&recvthread, NULL, recvthreadProc, &receiver);

	while (1) {
		char cmdbuf[256];
		unsigned char sendbuf[1024] = { 0x01, 0x05, 0x00, 0x00, 0x80, 0x28, 0x00, 0x7f, 0x40, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
		//unsigned char sendbuf[1024] = { 0x01, 0x05, 0x00, 0x00, 0x80, 0x28, 0x00, 0x7f, 0x40, 0x00, 0x1f, 0x99, 0x00, 0x00, 0x00, 0x00 };
		unsigned char *pos = &sendbuf[16];

		printf("AT COMMAND : ");
		gets(cmdbuf);

		sendbuf[3] = strlen(cmdbuf) + 2;
		memcpy(pos, cmdbuf, strlen(cmdbuf));
		pos += strlen(cmdbuf);
		*(pos++) = 0x0d;
		*(pos++) = 0;

		printf("SEND : \n");
		hexDump(sendbuf, sendbuf[3] + 16);
		printf("\n");

		send(sock, (const char*)sendbuf, sendbuf[3] + 16, 0);
	}
	
	return 0;
}