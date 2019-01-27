#include"socks5.hpp"
void Socks5Server::ConnectEventHandle(int connectfd)
{
		TraceLog("new connect event:%d", connectfd);
		//设置描述符为非阻塞
		//添加connectfd到epoll，监听的读事件
		SetNonblocking(connectfd);
		OPEvent(connectfd,EPOLLIN,EPOLL_CTL_ADD);
		//建立客户端通道
		Connect* con = new Connect;
		con->_state = AUTH;
		con->_clientChannel._fd = connectfd;
		_fdConnectMap[connectfd] = con;
		con->_ref++;
		//一个连接有两个描述符映射，一左一右，map里有两个文件描述符
		//当计数ref减到0，代表两个通道都关了，这时才delete销毁这个连接
}
int Socks5Server::AuthHandle(int connectfd)
{
		//大于0：recv如果读取成功返回实际读取到的字节数
		//等于0：对端关闭返回0（引发四次挥手）
		//小于0：出错
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
		if (buf[0] != 0x05)
		{
				ErrorLog("not socks5");
				reply[1] = 0xFF;
				ret = -1;
		}

		if(send(connectfd, reply, 2, 0) != 2)
		{
				ErrorLog("reply auth");
		}

		return ret;
}//失败返回-1
//数据没到返回-2
//连接成功返回serverfd
int Socks5Server::EstablishmentHandle(int connectfd)
{
		char buf[280];
		int rlen = recv(connectfd, buf, 280, MSG_PEEK);
		if (rlen < 10)
		{
				TraceLog("socks5 auth header : %d", rlen);
				return 0;
		}

		recv(connectfd, buf, 4, 0);
		char ip[4];
		char port[2];

		int addrType = buf[3];
		if (addrType == 0x01) 
		{
				recv(connectfd, ip, 4, 0);

				recv(connectfd, port, 2, 0);


				TraceLog("ip:%s", ip);
		}
		else if (addrType == 0x03) 
		{

				char len;
				recv(connectfd, &len, 1, 0);

				recv(connectfd, buf, len, 0);
				buf[len] = '\0';

				TraceLog("domain:%s", buf);

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

		}
		else if (addrType == 0x04)
		{
				TraceLog("not support ipv6");
				return -1;
		}
		else
		{
				ErrorLog("error address type");
				return -1; 
		}

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


		SetNonblocking(serverfd);
		OPEvent(serverfd, EPOLLIN, EPOLL_CTL_ADD);

		Connect* con = _fdConnectMap[connectfd];
		con->_serverChannel._fd = serverfd;
		_fdConnectMap[serverfd] = con;
		con->_ref++;

		char reply[10] = {0};
		reply[0] = 0x05;
		reply[1] = 0x00;
		reply[2] = 0x00;
		reply[3] = 0x01;

		if(send(connectfd, reply, 10, 0) != 10)
		{
				ErrorLog("reply");
				return -1;
		}

		return 1;
}
void Socks5Server::ReadEventHandle(int connectfd) 
{
		//	TraceLog("read event:%d", connectfd);
		//分三步。读事件处理三次
		//1：身份认证2：建立连接3：发送数据
		//发送数据可以分多次发送，有可能回数据
		//但是不关心谁给谁发		
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

				Channel* clientChannel = &(con->_clientChannel);
				Channel* serverChannel = &(con->_serverChannel);
				if (connectfd == serverChannel->_fd)
				{
						swap(clientChannel, serverChannel);

				}

				Forwarding(clientChannel, serverChannel);
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
		Socks5Server server(8000);
		server.Start();
		return 0;
}
