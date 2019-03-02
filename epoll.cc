#include "epoll.h"

const size_t EpollServer::_MAX_EVENTS = 10000;

void EpollServer::Start()
{
	_listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenfd < 0)
	{
		ErrorLog("socket");
		return;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(_listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		ErrorLog("bind");
		return;
	}

	if (listen(_listenfd, 10000) < 0)
	{
		ErrorLog("listen");
		return;
	}

	TraceLog("listen on %d", _port);

	_eventfd = epoll_create(_MAX_EVENTS);
	if (_eventfd < 0)
	{
		ErrorLog("epoll_create");
		return;
	}

	// 将监听套接字添加到epoll
	SetNonblocking(_listenfd);
	OpEvent(_listenfd, EPOLLIN, EPOLL_CTL_ADD);

	// 事件循环
	EventLoop();
}

void EpollServer::EventLoop()
{
	struct epoll_event events[_MAX_EVENTS];
	while (1)
	{
		int n = epoll_wait(_eventfd, events, _MAX_EVENTS, -1);
		if (n < 0)
		{
			ErrorLog("epoll_wait");
			break;
		}

		for (int i = 0; i < n; ++i)
		{
			if(events[i].data.fd == _listenfd)
			{
				struct sockaddr_in addr;
				socklen_t len;
				int connectfd = accept(_listenfd, (struct sockaddr*)&addr, &len);
				if (connectfd < 0)
				{
					ErrorLog("accept");
					continue;
				}

				ConnectEventHandle(connectfd);
			}
			else if (events[i].events & EPOLLIN)
			{
				ReadEventHandle(events[i].data.fd);
			}
			else if (events[i].events & EPOLLOUT)
			{
				WriteEventHandle(events[i].data.fd);
			}
			else if (events[i].events & EPOLLERR)
			{
				TraceLog("error event");
			}
			else
			{
				ErrorLog("unknow event");
			}
		}
	}
}

void EpollServer::RemoveConnect(int fd)
{
	TraceLog("%d", fd);

	// 删除掉对应的事件
	OpEvent(fd, 0, EPOLL_CTL_DEL);

	Connect* con = _fdConnectMap[fd];
	if (--con->_ref == 0)
	{
		delete con;
	}

	// 删除对应的通道
	_fdConnectMap.erase(fd);
}

void EpollServer::SendInLoop(int fd, const char* buf, int len)
{
	int slen = send(fd, buf, len, 0);
	if (slen < 0)
	{
		ErrorLog("send");
		return;
	}

	if (slen < len)
	{
		TraceLog("len:%d, slen:%d", len, slen);
		map<int, Connect*>::iterator it = _fdConnectMap.find(fd);
		if (it != _fdConnectMap.end())
		{
			Connect* con = it->second;
			Channel* channel = &(con->_clientChannel);
			if (fd == con->_serverChannel._fd)
			{
				channel = &(con->_serverChannel);
			}

			channel->_buffer.append(buf+slen);
			int event = 0;
			event |= EPOLLIN;
			event |= EPOLLOUT;
			event |= EPOLLONESHOT;
			OpEvent(fd, event, EPOLL_CTL_MOD);
		}
	}
}

void EpollServer::Forwarding(Channel* clientChannel, Channel* serverChannel,
							 bool recvDecrypt, bool sendEncry)
{
	char buf[4097];
	int rlen = recv(clientChannel->_fd, buf, 4096, 0);
	if (rlen < 0)
	{
		ErrorLog("recv");
		return;
	}
	else if (rlen == 0)
	{
		RemoveConnect(clientChannel->_fd);
		shutdown(serverChannel->_fd, SHUT_WR);
		return;
	}

	if (recvDecrypt)
	{
		Decrypt(buf, rlen);
	}

	if (sendEncry)
	{
		Encry(buf, rlen);
	}

	buf[rlen] = '\0';
	SendInLoop(serverChannel->_fd, buf, rlen);
}
