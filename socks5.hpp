#ifndef __SOCKS5_HPP__
#define __SOCKS5_HPP__

#include"epoll.hpp"
class Socks5Server : public EpollServer
{
		public:
				Socks5Server(int port)
                  :EpollServer(port)
{}
int AuthHandle(int connectfd);
int EstablishmentHandle(int connectfd);
void Forwarding(Channel* clientChannel,Channel* serverChannel);
void RemoveConnect(int fd);
void SendInLoop(int fd,const char* buf,int len);
virtual void ConnectEventHandle(int connectfd) ;
virtual void ReadEventHandle(int connectfd) ;
virtual void WriteEventHandle(int connectfd) ;
};

#endif

