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
		AUTH,			// 身份认证
		ESTABLISHMENT,	// 建立连接
		FORWARDING,		// 转发模式
	};

	// 通道
	struct Channel
	{
		int _fd;
		string _buffer; // 写缓冲
	};

	// 连接
	struct Connect
	{
		State _state;			// socks5连接状态
		Channel _clientChannel; // 客户端通道
		Channel _serverChannel;	// 服务端通道

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
	int _listenfd;  // 监听描述符
	int _port;		// 监听端口

	int _eventfd;	// 事件描述符
	static const size_t _MAX_EVENTS; // 最大事件数量

	map<int, Connect*> _fdConnectMap;
};


#endif //__EPOLL_H__