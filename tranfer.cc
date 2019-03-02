#include "tranfer.h"

void TranferServer::ConnectEventHandle(int connectfd)
{
	int serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if(connect(serverfd, (struct sockaddr*)&_socks5addr, sizeof(_socks5addr)) < 0)
	{
		ErrorLog("connect socks5 server");
		return;
	}

	SetNonblocking(connectfd);
	OpEvent(connectfd, EPOLLIN, EPOLL_CTL_ADD);

	SetNonblocking(serverfd);
	OpEvent(serverfd, EPOLLIN, EPOLL_CTL_ADD);

	Connect* con = new Connect;
	con->_clientChannel._fd = connectfd;
	con->_ref++;
	_fdConnectMap[connectfd] = con;

	con->_serverChannel._fd = serverfd;
	con->_ref++;
	_fdConnectMap[serverfd] = con;
	con->_state = FORWARDING;
}

void TranferServer::ReadEventHandle(int connectfd)
{
	map<int, Connect*>::iterator it = _fdConnectMap.find(connectfd);
	if (it != _fdConnectMap.end())
	{
		Connect* con = it->second;
		bool recvDecrypt = false, sendEncry = true;
		Channel* clinetChannel = &(con->_clientChannel);
		Channel* serverChannel = &(con->_serverChannel);
		if (connectfd == con->_serverChannel._fd)
		{
			swap(recvDecrypt, sendEncry);
			swap(clinetChannel, serverChannel);
		}

		Forwarding(clinetChannel, serverChannel, recvDecrypt, sendEncry);
	}
	else
	{
		ErrorLog("invalid fd:%d", connectfd);
	}
}

void TranferServer::WriteEventHandle(int connectfd)
{
	TraceLog("%d", connectfd);
	map<int, Connect*>::iterator it = _fdConnectMap.find(connectfd);
	if (it != _fdConnectMap.end())
	{
		Connect* con = it->second;
		Channel* channel = &(con->_clientChannel);
		if (connectfd == con->_serverChannel._fd)
		{
			channel = &(con->_serverChannel);
		}

		string buffer;
		buffer.swap(channel->_buffer);
		SendInLoop(connectfd, buffer.c_str(), buffer.size());
	}
	else
	{
		ErrorLog("invalid fd:%d", connectfd);
	}
}

int main()
{
	TranferServer server(8000, "192.168.5.159", 8001);
	server.Start();
}
