#include "socks5.h"

void Socks5Server::ConnectEventHandle(int connectfd)
{
	TraceLog("%d", connectfd);

	// 设置描述符为非阻塞，并添加到epoll
	SetNonblocking(connectfd);
	OpEvent(connectfd, EPOLLIN, EPOLL_CTL_ADD);

	// 建立客户端通道
	Connect* con = new Connect;
	con->_state = AUTH;
	con->_clientChannel._fd = connectfd;
	_fdConnectMap[connectfd] = con;
	con->_ref++;
}

int Socks5Server::AuthHandle(int connectfd)
{
	/*	+----+----------+----------+
		|VER | NMETHODS | METHODS  |
		+----+----------+----------+
		| 1  |    1     | 1 to 255 |
		+----+----------+----------+

		+----+--------+
		|VER | METHOD |
		+----+--------+
		| 1  |   1    |
		+----+--------+*/

	int ret = 1;
	char buf[258];
	char reply[2];
	reply[0] = 0x05;
	reply[1] = 0x00;

	int rlen = recv(connectfd, buf, 258, MSG_PEEK);
	if (rlen <= 0)
	{
		RemoveConnect(connectfd);
		return -1;
	}
	else if (rlen < 3)
	{
		TraceLog("socks5 auth header : %d", rlen);
		return 0;
	}


	recv(connectfd, buf, rlen, 0);
	Decrypt(buf, rlen);
	if (buf[0] != 0x05)
	{
		ErrorLog("not socks5");
		reply[1] = 0xFF;
		ret = -1;
	}

	Encry(reply, 2);
	if(send(connectfd, reply, 2, 0) != 2)
	{
		ErrorLog("reply auth");
	}

	return ret;
}

int Socks5Server::EstablishmentHandle(int connectfd)
{
		//+----+-----+-------+------+----------+----------+
		//|VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
		//+----+-----+-------+------+----------+----------+
		//| 1  |  1  | X'00' |  1   | Variable |    2     |
		//+----+-----+-------+------+----------+----------+

	char buf[280];
	int rlen = recv(connectfd, buf, 280, MSG_PEEK);
	if (rlen < 10)
	{
		TraceLog("socks5 auth header : %d", rlen);
		return 0;
	}

	recv(connectfd, buf, 4, 0);
	Decrypt(buf, 4);

	char ip[4];
	char port[2];

	int addrType = buf[3];
	if (addrType == 0x01) // ipv4
	{
		recv(connectfd, ip, 4, 0);
		Decrypt(ip, 4);
		recv(connectfd, port, 2, 0);
		Decrypt(port, 4);

		TraceLog("ip:%s", ip);
	}
	else if (addrType == 0x03) // domain
	{
		// 先通过第一个字段获取长度
		char len;
		recv(connectfd, &len, 1, 0);
		Decrypt(&len, 1);

		// 再去获取域名
		recv(connectfd, buf, len, 0);
		buf[len] = '\0';

		TraceLog("encry domain:%s", buf);
		Decrypt(buf, len);
		TraceLog("decrypt domain:%s", buf);

		struct hostent* haddr = gethostbyname(buf);
		if (haddr == NULL)
		{
			ErrorLog("gethostbyname");
			return -1;
		}
		else
		{
			struct in_addr addr;
			memcpy(ip, haddr->h_addr, haddr->h_length);
		}
		recv(connectfd, port, 2, 0);
		Decrypt(port, 2);
	}
	else if (addrType == 0x04) // ipv6
	{
		TraceLog("not support ipv6");
		return -1;
	}
	else
	{
		ErrorLog("error address type");
		return -1; 
	}

	// 连接服务器
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	memcpy(&addr.sin_addr.s_addr, ip, 4);
	addr.sin_port = *((uint16_t*)port);
	int serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if(connect(serverfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		ErrorLog("connect server error");
		return -1;
	}

	// 建立server channel
	SetNonblocking(serverfd);
	OpEvent(serverfd, EPOLLIN, EPOLL_CTL_ADD);

	Connect* con = _fdConnectMap[connectfd];
	con->_serverChannel._fd = serverfd;
	_fdConnectMap[serverfd] = con;
	con->_ref++;

	char reply[10] = {0};
	reply[0] = 0x05;
	reply[1] = 0x00;
	reply[2] = 0x00;
	reply[3] = 0x01;

	Encry(reply, 10);
	if(send(connectfd, reply, 10, 0) != 10)
	{
		ErrorLog("reply");
		return -1;
	}

	return 1;
}

void Socks5Server::ReadEventHandle(int connectfd)
{
	//Connect* con = _fdConnectMap[connectfd];
	map<int, Connect*>::iterator it =_fdConnectMap.find(connectfd);
	if (it == _fdConnectMap.end())
	{
		ErrorLog("invalid fd:%d", connectfd);
		return;
	}

	Connect* con = it->second;
	if (con->_state == AUTH)
	{
		int ret = AuthHandle(connectfd);
		if(ret == 1)
			con->_state = ESTABLISHMENT;
	}
	else if (con->_state == ESTABLISHMENT)
	{
		int ret = EstablishmentHandle(connectfd);
		if (ret == 1)
			con->_state = FORWARDING;
	}
	else
	{
		bool recvDecrypt = true, sendEncry = false;
		Channel* clientChannel = &(con->_clientChannel);
		Channel* serverChannel = &(con->_serverChannel);
		if (connectfd == serverChannel->_fd)
		{
			swap(clientChannel, serverChannel);
			swap(recvDecrypt, sendEncry);
		}

		Forwarding(clientChannel, serverChannel, recvDecrypt, sendEncry);
	}
}

void Socks5Server::WriteEventHandle(int connectfd)
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
	Socks5Server server(8001);
	server.Start();

	return 0;
}
