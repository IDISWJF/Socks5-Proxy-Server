#ifndef __TRANFER_H__
#define __TRANFER_H__

#include "epoll.h"

class TranferServer : public EpollServer 
{
public:
	TranferServer(int selfPort, const char* socks5Ip, int socks5Port)
		:EpollServer(selfPort)
	{
		memset(&_socks5addr, 0, sizeof(struct sockaddr_in));
		_socks5addr.sin_family = AF_INET;
		_socks5addr.sin_port = htons(socks5Port);
		_socks5addr.sin_addr.s_addr = inet_addr(socks5Ip);
	}

	virtual void WriteEventHandle(int connectfd);
	virtual void ReadEventHandle(int connectfd);
	virtual void ConnectEventHandle(int connectfd);
protected:
	struct sockaddr_in _socks5addr;
};


#endif //__TRANFER_H__