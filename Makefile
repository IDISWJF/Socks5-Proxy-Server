.PHONY:all
all:socks5 tranfer
socks5:socks5.cc epoll.cc
	g++ -o socks5 socks5.cc epoll.cc -std=c++11
tranfer:tranfer.cc epoll.cc
	g++ -o tranfer tranfer.cc epoll.cc -std=c++11
