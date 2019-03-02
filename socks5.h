
#ifndef __SOCKS5_H__
#define __SOCKS5_H__

#include "epoll.h"

class Socks5Server : public EpollServer 
{
public:
	Socks5Server(int port)
		:EpollServer(port)
	{}

	// 返回
	//	1 成功
	//  0 数据未到齐
	// -1 失败
	int AuthHandle(int connectfd);
	int EstablishmentHandle(int connectfd);

	virtual void WriteEventHandle(int connectfd);
	virtual void ReadEventHandle(int connectfd);
	virtual void ConnectEventHandle(int connectfd);
};


#endif //__SOCKS5_H__