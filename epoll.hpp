#ifndef __EPOLL_HPP__
#define __EPOLL_HPP__
#include"common.hpp"
//将打开的网页关闭会触发sigpipe
//所以需要忽略此信号
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
				//枚举运行过程中的转态
				enum State
				{
						AUTH,//身份认证
						ESTABLISHMENT,//建立连接
						FORWARDING,//转发
				};
				//一个连接由两个通道构成
				struct Channel
				{
						int _fd;//描述符
						string _buffer;//写缓冲
						////		Channel()
						////			:_fd(0)
						////	{}
				};
				struct Connect
				{
						State _state;//连接状态
						Channel _clientChannel;
						Channel _serverChannel;
						int _ref;

						Connect()
								:_state(AUTH)
								 ,_ref(0)
						{}
				};
				EpollServer(int port = 8000)
						:_port(port)
						 ,_listenfd(-1)
						 ,_eventfd(-1)
		{}
				~EpollServer()
				{
						if(_listenfd != -1)
								close(_listenfd);
				}
				// 设置非阻塞
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
				void OPEvent(int fd,int events,int op)
				{
						struct epoll_event event;
						event.events = events;
						event.data.fd = fd;
						if(epoll_ctl(_eventfd,op,fd,&event) < 0)
						{
								ErrorLog("epoll_ctl.fd:%d+op:%d",fd,op);
						}
				}
				void Start();
				void EventLoop();

	void Forwarding(Channel* clientChannel, Channel* serverChannel);
	void RemoveConnect(int fd);
	void SendInLoop(int fd, const char* buf, int len);
				//多态实现的虚函数
				virtual void ConnectEventHandle(int connectfd) = 0;
				virtual void ReadEventHandle(int connectfd) = 0;
				virtual void WriteEventHandle(int connectfd) = 0;
		protected:
				int _port;      //端口
				int _listenfd;  //监听描述符
				int _eventfd;   //事件描述符

				map<int ,Connect*> _fdConnectMap;//fd映射连接的map容器
};
#endif
