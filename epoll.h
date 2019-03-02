#ifndef __EPOLL_H__
#define __EPOLL_H__

#include "common.h"
#include "encry.h"

class IgnoreSigPipe
{
public:
	IgnoreSigPipe()
	{
		::signal(SIGPIPE, SIG_IGN);
	}
};

static IgnoreSigPipe initISP;

class EpollServer
{
public:
	enum State
	{
		AUTH,			// �����֤
		ESTABLISHMENT,	// ��������
		FORWARDING,		// ת��ģʽ
	};

	// ͨ��
	struct Channel
	{
		int _fd;
		string _buffer; // д����
	};

	// ����
	struct Connect
	{
		State _state;			// socks5����״̬
		Channel _clientChannel; // �ͻ���ͨ��
		Channel _serverChannel;	// �����ͨ��

		int _ref;

		Connect()
			:_ref(0)
			,_state(AUTH)
		{}
	};

	EpollServer(int port)
		:_port(port)
		,_eventfd(-1)
		,_listenfd(-1)
	{}

	~EpollServer()
	{
		if (_listenfd != -1)
			close(_listenfd);
	}

	void SetNonblocking(int sfd)
	{
		int flags, s;
		flags = fcntl (sfd, F_GETFL, 0);
		if (flags == -1)
			ErrorLog("SetNonblocking:F_GETFL");

		flags |= O_NONBLOCK;
		s = fcntl (sfd, F_SETFL, flags);
		if (s == -1)
			ErrorLog("SetNonblocking:F_SETFL");
	}

	void OpEvent(int fd, int events, int op)
	{
		struct epoll_event event;
		event.events = events;
		event.data.fd = fd;
		if(epoll_ctl(_eventfd, op, fd, &event) < 0)
		{
			ErrorLog("epoll_ctl.fd:%d+op:%d", fd, op);
		}
	}

	void Start();
	void EventLoop();

	virtual void WriteEventHandle(int connectfd) = 0;
	virtual void ReadEventHandle(int connectfd) = 0;
	virtual void ConnectEventHandle(int connectfd) = 0;

	void Forwarding(Channel* clientChannel, Channel* serverChannel,
		bool recvDecrypt, bool sendEncry);
	void RemoveConnect(int fd);
	void SendInLoop(int fd, const char* buf, int len);

protected:
	int _listenfd;  // ����������
	int _port;		// �����˿�

	int _eventfd;	// �¼�������
	static const size_t _MAX_EVENTS; // ����¼�����

	map<int, Connect*> _fdConnectMap;
};


#endif //__EPOLL_H__