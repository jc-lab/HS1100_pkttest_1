#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#elif defined(__linux)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <vector>

class JsSimpleSocketReceiverBase
{
private:
	volatile int m_runstate;

public:
	enum RecvProcessResult
	{
		RECVRESULT_ERROR = -1,
		RECVRESULT_DONE = 0,
		RECVRESULT_RETRY = 1,
	};

	virtual RecvProcessResult recvProcess(const char *buffer, int recvlen, int *processedsize) = 0;
	virtual void close() {}

protected:
	virtual int nativeRecv(char *buffer, int bufsize) = 0;

public:
	JsSimpleSocketReceiverBase();
	virtual ~JsSimpleSocketReceiverBase();
	int run();
};

template <typename TSOCK>
class JsSimpleSocketReceiver : public JsSimpleSocketReceiverBase
{
private:
	TSOCK m_sock;

public:
	JsSimpleSocketReceiver()
	{
		m_sock = NULL;
	}

	JsSimpleSocketReceiver(TSOCK sock)
	{
		m_sock = sock;
	}

	virtual ~JsSimpleSocketReceiver()
	{
		close();
	}

	void close() override
	{
		if (m_sock != NULL)
		{
			closesocket(m_sock);
			m_sock = NULL;
		}
	}

	int nativeRecv(char *buffer, int bufsize) override
	{
		return recv(m_sock, buffer, bufsize, 0);
	}
};