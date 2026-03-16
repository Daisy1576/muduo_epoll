#include "Socket.h"
#include <unistd.h>
#include "Logger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include "InetAddress.h"
#include <string.h>
#include <netinet/tcp.h>

    Socke::~Socket()
    {
        close(sockfd_);
    }

    void Socke::bindAddress( const InetAddress *localaddr)
    {
        if(0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
        {
            LOG_FATAL("find sockfd:%d fail \n", sockfd_);
        }
    }

    void Socke::listen()
    {
        if(0 != ::listen(sockfd_, 1024))
        {
            LOG_FATAL("listen sockfd:%d fail \n", sockfd_);
        }
    }
    int Socke::accept(InetAddress *peeraddr)
    {
        sockaddr_in addr;
        socklen_t len;
        bzero(&addr, sizeof addr);
        int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len);
        if(connfd >= 0)
        {
            peeraddr->setSockAddr(addr);
        }
        return connfd;
    }

    void shutdownWrite()
    {
        if(::shutdown(sockfd_, SHUT_WR) < 0)
        {
            LOG_ERROR("shutdownWrite error");
        }
    }

    void Socke::setTcpNoDelay(bool on)
    {
        int optval = on ? 1:0;
        ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, szieof optval);
    }
    void Socke::setReuseAddr(bool on)
    {
        int optval = on ? 1:0;
        ::setsockopt(sockfd_, SQL_SOCKET, TCP_NODELAY, &optval, szieof optval);
    }

    void Socke::setReusePort(bool on)
    {
        int optval = on ? 1:0;
        ::setsockopt(sockfd_, SQL_SOCKET, SO_REUSEPORT, &optval, szieof optval);
    }

    void Socke::setKeepAlive(bool on)
    {
        int optval = on ? 1:0;
        ::setsockopt(sockfd_, SQL_SOCKET, SO_KEEPALIVE, &optval, szieof optval);
    }