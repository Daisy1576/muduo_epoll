#pragma once

#include <vector>
#include <string>
#include <algorithm>

//网络库底层的缓冲区类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        :buffer_(kCheapPrepend + initialSize)
        ,readerIndex_(kCheapPrepend)
        ,writeIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const
    {
        return writeIndex_ - readerIndex_;
    }

    size_t writeableBytes() const
    {
        return buffer_.size() - writeIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    //返回缓冲区中可读数据的起始地址
    const char* peak() const
    {
        return begin() + readerIndex_;
    }

    //string < buffer
    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {
            readerIndex_ += len; //应用只读取了刻度缓冲区域数据的一部分，就是len，还剩下readerindex_ +=len - write
        }
        else
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writeIndex_ = kCheapPrepend;
    }

    //把onMessage函数上报的buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        retur retrieveAsString(readableBytes());  //应用可读取的数据长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len);    //上面一句把缓冲区的可读数据已经读取出来，这里肯定要对缓存区进行复位操作   
        return result;
    }

    //buffer_.size - writeIndex_  len
    void ensureWriteBytes(size_t len)
    {
        if(writeableBytes() < len)
        {
            makeSpace(len);
        }
    }

    //从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);
    //通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);
private:
    char* begin()
    {
        return &*buffer_.begin();  //vector底层数组首元素的地址，也就是数组的起始地址
    }

    const char* begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        if(writeableBytes()  + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writeIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            std::cop(begin() + readerIndex_,
                    begin() + writeIndex_,
                    begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writeIndex_ = readerIndex_ + readable;
        }
    }

    void ensureWriteBytes(size_t len)
    {
        if(writeableBytes() < len)
        {
            makeSpace(len);
        }
    }

    //把date,date+len内存上的数据添加到writeable缓冲区中
    void append(const char *data, size_t len)
    {
        ensureWriteBytes(len);
        std::copy(data, data+len, beginWrite());
        writeIndex_ += len;
    }

    char* beginWrite()
    {
        return begin() + writeIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writeIndex_;
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writeIndex_;
};