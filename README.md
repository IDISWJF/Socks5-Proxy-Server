# Socks5-Proxy-Server
Project function:

Accessing Web pages that cannot be accessed directly through proxy servers

Project Description:

The browser sends the data packet to the transmission channel and forwards it to the proxy server through the encryption of the transmission channel.

After authentication, the proxy server establishes a connection, connects the destination server, decrypts and forwards the data.

The destination server returns the result to the proxy server, encrypts it and forwards it to the local transmission channel.

After decrypting the transmission channel, the result is returned to the browser.

Knowledge points used in the project:

C++, STL library, SOCKS5 protocol, Epoll
