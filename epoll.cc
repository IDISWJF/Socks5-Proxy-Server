#include"epoll.hpp"
void EpollServer::Start()
{
		_listenfd = socket(AF_INET,SOCK_STREAM,0);
		if(_listenfd == -1)
		{
				ErrorLog("socket");
				return ;
		}
		struct sockaddr_in addr;
		memset(&addr,0,sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(_port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);//一台主机可能有多个网卡，有多个IP地址，INADDR_ANY监听本机的任意网卡
		if( bind(_listenfd,(struct sockaddr*)&addr,sizeof(addr)) < 0 )
		{
				ErrorLog("bind");
				return ;
		}
		if(listen(_listenfd,10000) < 0)
		{
				ErrorLog("listen");
				return ;
		}

		TraceLog(" listen on %d ",_port);

		_eventfd = epoll_create(100000);
		if(_eventfd == -1)
		{
				ErrorLog("epoll_create");
				return ;
		}
		//添加listenfd到epoll，监听连接事件
		OPEvent(_listenfd,EPOLLIN,EPOLL_CTL_ADD);
		//进入事件循环
		EventLoop();	
}
void EpollServer::EventLoop()
{
		struct epoll_event events[100000];
		while(1)
		{
				int n = epoll_wait(_eventfd,events,100000,-1);
				if(n < 0)
				{
						ErrorLog("epoll_wait");
						break;
				}
				for(int i = 0;i < n;++i)
				{
						if(events[i].data.fd == _listenfd)
						{
								struct sockaddr_in addr;
								socklen_t len;
								int connectfd = accept(_listenfd,(struct sockaddr*)&addr,&len);
								if(connectfd < 0)
								{
										ErrorLog("accept");
										continue;
								}
								TraceLog("new connect");
								ConnectEventHandle(connectfd);
						}
						else if(events[i].events & EPOLLIN)
						{
								ReadEventHandle(events[i].data.fd);
						}
						else if(events[i].events & EPOLLOUT)
						{
								WriteEventHandle(events[i].data.fd);
						}
						else if(events[i].events & EPOLLERR)
						{
								TraceLog("error event");
						}
						else
						{
								ErrorLog("unkonw event");
						}
				}
		}
}
