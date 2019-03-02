
#ifndef __SOCKS5_H__
#define __SOCKS5_H__

#include "epoll.h"

class Socks5Server : public EpollServer 
{
public:
	Socks5Server(int port)
		:EpollServer(port)
	{}

	// ����
	//	1 �ɹ�
	//  0 ����δ����
	// -1 ʧ��
	int AuthHandle(int connectfd);
	int EstablishmentHandle(int connectfd);

	virtual void WriteEventHandle(int connectfd);
	virtual void ReadEventHandle(int connectfd);
	virtual void ConnectEventHandle(int connectfd);
};


#endif //__SOCKS5_H__