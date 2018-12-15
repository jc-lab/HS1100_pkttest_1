#include "JsSimpleSocketReceiver.h"

#include <stdio.h>

JsSimpleSocketReceiverBase::JsSimpleSocketReceiverBase()
{

}

JsSimpleSocketReceiverBase::~JsSimpleSocketReceiverBase()
{
	close();
}

int JsSimpleSocketReceiverBase::run()
{
	int recvlen = 0;
	std::vector<unsigned char> queueBuffer;
	
	m_runstate = 1;

	while (m_runstate == 1) {
		int res;
		char recvbuf[1024];
		recvlen = nativeRecv(recvbuf, sizeof(recvbuf));

		if (recvlen <= 0)
			break;

		queueBuffer.insert(queueBuffer.end(), &recvbuf[0], &recvbuf[recvlen]);

		do {
			int procsize = 0;
			res = recvProcess((const char*)&queueBuffer[0], queueBuffer.size(), &procsize);
			if (procsize > 0 && procsize < queueBuffer.size()) {
				int endpos = queueBuffer.size();
				queueBuffer.push_back(0);
				queueBuffer.assign(&queueBuffer[procsize], &queueBuffer[endpos]);
			} else if (procsize > 0) {
				queueBuffer.clear();
			}
		} while ((res == RECVRESULT_RETRY) && (queueBuffer.size() > 0));
		if (res == RECVRESULT_ERROR)
			break;
	}

	if (recvlen > 0)
		return 1;
	return recvlen;
}
