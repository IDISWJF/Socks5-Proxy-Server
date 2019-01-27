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
virtual void ConnectEventHandle(int connectfd) ;
virtual void ReadEventHandle(int connectfd) ;
virtual void WriteEventHandle(int connectfd) ;
};

#endif

