
.PHONY:all
all:socks5 tranfer

socks5:socks5.cc epoll.cc 
	g++ -o socks5 socks5.cc epoll.cc -std=c++11 

.PHONY:tranfer
tranfer:
	g++ -o tranfer tranfer.cc epoll.cc -std=c++11
.PHONY:clean
clean:
	rm -f socks5 tranfer                             
