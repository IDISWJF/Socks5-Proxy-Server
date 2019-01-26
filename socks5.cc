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
		char buf[260];
		int rlen = recv(connectfd, buf ,260,MSG_PEEK);
		if(rlen <= 0)
		{
				return -1;
		}
		else if(rlen < 3)
		{
				return 0;
		}
		else
		{
				//大于3，有多少读多少
				recv(connectfd,buf,rlen,0);
				if(buf[0] != 0x05)
				{
						ErrorLog("not socks5");
						return -1;
				}
				return 1;
		}
}
//失败返回-1
//数据没到返回-2
//连接成功返回serverfd
int Socks5Server::EstablishmentHandle(int connectfd)
{
		char buf[256];
		int rlen = recv(connectfd,buf,256,MSG_PEEK);
		if(rlen <= 0)
		{
				TraceLog("rlen :%d",rlen);
				return -1;
		}
		else if(rlen < 10)
		{
				return -2;
		}
		else
		{
				char ip[4];
				char port[2];

				recv(connectfd, buf ,4, 0);
				char addresstype = buf[3];
				if(addresstype == 0x01)//ipv4
				{
						recv(connectfd,ip,4,0);
						recv(connectfd,port,2,0);
				}
				else if(addresstype == 0x03)//domainname域名
				{
						char len = 0;
						//recv DOMAINNAME
						recv(connectfd,&len,1,0);
						recv(connectfd,buf,len,0);

						//recv port
						recv(connectfd,port,2,0);

						buf[len] = '\0';
						TraceLog("DOMAINNAME:%s",buf);

						struct hostent* hostptr = gethostbyname(buf);
						memcpy(ip, hostptr->h_addr,hostptr->h_length);
				}
				else if(addresstype == 0x04)//ipv6
				{
						ErrorLog("not support ipv6");
						return -1;
				}
				else
				{
						ErrorLog("invalid address type");
						return -1;
				}

				struct sockaddr_in addr;
				memset(&addr, 0, sizeof(struct sockaddr_in));
				addr.sin_family = AF_INET;
				memcpy(&addr.sin_addr.s_addr, ip, 4);
				addr.sin_port = *((uint16_t*)port);

				int serverfd = socket(AF_INET, SOCK_STREAM, 0);
				if(serverfd < 0)
				{
						ErrorLog("socket");
						return -1;
				}
				if (connect(serverfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
				{
						ErrorLog("connect error");
						close(serverfd);
						return -1;
				}
				return serverfd;
		}
}
void Socks5Server::Forwarding(Channel* clientChannel,Channel* serverChannel)
{
		char buf[4096];
		int rlen = recv(clientChannel->_fd, buf,4096, 0);
		if(rlen < 0)
		{
				ErrorLog("recv:%d",clientChannel->_fd);
		}
		else if(rlen == 0)
		{
				//client channel 发起关闭
				shutdown(serverChannel->_fd,SHUT_WR);
				RemoveConnect(clientChannel->_fd);
				return;
		}
		else
		{
				buf[rlen] = '\0';
				SendInLoop(serverChannel->_fd,buf, rlen);
		}
}
void Socks5Server::ReadEventHandle(int connectfd) 
{
		//	TraceLog("read event:%d", connectfd);
		map<int,Connect*>::iterator it = _fdConnectMap.find(connectfd);
		//分三步。读事件处理三次
		//1：身份认证2：建立连接3：发送数据
		//发送数据可以分多次发送，有可能回数据
		//但是不关心谁给谁发		
		if(it != _fdConnectMap.end())
		{
				Connect* con = it->second;
				if(con->_state == AUTH)
				{
						char reply[2];
						reply[0] = 0x55;
						int ret = AuthHandle(connectfd);
						if(ret == 0)
						{
								return;
						}
						else if(ret == 1)
						{
								reply[1] = 0x00;
								con->_state = ESTABLISHMENT;
						}
						else if(ret == -1)
						{
								reply[1] = 0xFF;
								RemoveConnect(connectfd);
						}
						if(send(connectfd, reply, 2,0) != 2)
						{
								ErrorLog("auth reply");
						}
				}
				else if(con->_state == ESTABLISHMENT)
				{
						TraceLog("forwarding");						
						char reply[10] = {0};
						reply[0] = 0x05;
						int serverfd = EstablishmentHandle(connectfd);
						if(serverfd == -1)
						{
								reply[1] = 0x01;							
								//RemoveConnect(connectfd);//连接失败需要删除连接，因为连接中还建立着各种东西
								return;
						}
						else if(serverfd == -2)
						{
								return ;
						}
						else
						{
								reply[1] = 0x00;
								reply[3] = 0x01;
						}

						if(send(connectfd,reply,10,0) != 10)
						{
								ErrorLog("Establishment reply");
						}
						if(serverfd >= 0)
						{
								SetNonblocking(serverfd);
								OPEvent(serverfd,EPOLLIN,EPOLL_CTL_ADD);
								con->_serverChannel._fd = serverfd;
								_fdConnectMap[serverfd] = con;
								con->_ref++;						
								con->_state = FORWARDING;
						}
				}
				else if(con->_state == FORWARDING)
				{
						Channel* clientChannel = &con->_clientChannel;
						Channel* serverChannel = &con->_serverChannel;
						if(connectfd == serverChannel->_fd)
						{
								swap(clientChannel , serverChannel);
						}

						//client->server
						Forwarding(clientChannel, serverChannel);
				}
				else
				{
						assert(false);
				}
		}
}
void Socks5Server::RemoveConnect(int fd)
{
		OPEvent(fd,EPOLLIN,EPOLL_CTL_DEL);
		map<int,Connect*>::iterator it = _fdConnectMap.find(fd);
		if(it != _fdConnectMap.end())
		{
				Connect* con = it->second;
				if(--con->_ref == 0)
				{
						delete con;
						_fdConnectMap.erase(it);
				}
				else
				{
						assert(false);
				}
		}
}
void Socks5Server::SendInLoop(int fd,const char* buf,int len)
{
		int slen = send(fd, buf, len, 0);
		if(slen < 0)
		{
				ErrorLog("send to %d",fd);
		}
		else if(slen < len)
		{
				TraceLog("recv %d bytes, send %d bytes,left %d send in loop",len,slen,len-slen);
				map<int,Connect*>::iterator it = _fdConnectMap.find(fd);
				if(it != _fdConnectMap.end())
				{
						Connect* con = it->second;
						Channel* channel = &con->_clientChannel;
						if(fd == con->_serverChannel._fd)
						{
								channel = &con->_serverChannel;
						}

						int events = EPOLLOUT | EPOLLIN | EPOLLONESHOT;
						OPEvent(fd , events,EPOLL_CTL_MOD);
						channel->_buffer.append(buf+slen);
				}
				else
				{
						assert(false);}
		}
}
void Socks5Server::WriteEventHandle(int connectfd) 
{
		map<int,Connect*>::iterator it = _fdConnectMap.find(connectfd);
		if(it != _fdConnectMap.end())
		{
				Connect* con = it->second;
				Channel* channel = &con->_clientChannel;
				if(connectfd = con->_serverChannel._fd)
				{
						channel = &con->_serverChannel;
				}
				string buf;
				buf.swap(channel->_buffer);
				////			buf += channel->_buf;
				////			channel->_buf.clear();
				SendInLoop(connectfd,buf.c_str(),buf.size());
		}
		else
		{
				assert(connectfd);
		}
}
int main()
{
		Socks5Server server(8000);
		server.Start();
		return 0;
}
