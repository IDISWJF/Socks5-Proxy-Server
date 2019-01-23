#include"socks5.hpp"
void Socks5Server::ConnectEventHandle(int connectfd)
{
TraceLog("%d", connectfd);
}
void Socks5Server::ReadEventHandle(int connectfd) 
{
TraceLog("%d", connectfd);
}
void Socks5Server::WriteEventHandle(int connectfd) 
{
TraceLog("%d", connectfd);
}
int main()
{
		Socks5Server server(8080);
		server.Start();
		return 0;
}
