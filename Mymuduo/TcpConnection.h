#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"

#include <memory> 
#include <string>
#include <atomic>
#include <string>

class Channel;
class EventLoop;
class Socket;

class TcpConnection : noncopyable, public std::__enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                const std::string &name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected;}

      void send(const std::string &buf);
    void shutdown();

    // 设置连接状态变化的回调函数
    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }

    // 设置消息接收的回调函数
    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }

    // 设置写操作完成的回调函数
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }

    // 设置高水位标记回调函数（同时传入高水位阈值）
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    void setCloseCallback(const CloseCallback& cb)
    {
        closeCallback_ = cb;
    }

    void connectEstablished();
    void connectDestroyed();

    void setState(stateE state) 
    {
        state_  = state; 
    }
private:
    enum StateE {kDisconnectioned, kConnecting, kConnected, kDisconnecting};
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

  

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();


    EventLoop *loop_;
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;

    ConnectionCallback connectionCallback_;  //有新连接的回调
    MessageCallback messageCallback_;  //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;  //消息发送完的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;       //接受数据的缓冲区
    Buffer outputBuffer_;      //发送数据的缓冲区
};