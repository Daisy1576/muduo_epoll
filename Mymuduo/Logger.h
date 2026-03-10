#pragma once

#include <string>
#include <cstdio>    // 添加：snprintf 需要
#include <iostream>  // 添加：std::cout 需要
#include "noncopyable.h"
#include "Timestamp.h"

// 日志级别枚举
enum LogLevel {
    INFO,
    ERROR,
    FATAL,
    DEBUG
};

// 前置声明
class Logger;

// 宏定义（放在类外面！）
#define LOG_INFO(logmsgFormat, ...)                 \
    do {                                            \
        Logger &logger = Logger::instance();        \
        logger.setLogLevel(INFO);                   \
        char buf[1024] = {0};                       \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                            \
    } while(0)

#define LOG_ERROR(logmsgFormat, ...)                \
    do {                                            \
        Logger &logger = Logger::instance();        \
        logger.setLogLevel(ERROR);                  \
        char buf[1024] = {0};                       \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                            \
    } while(0)

#define LOG_FATAL(logmsgFormat, ...)                \
    do {                                            \
        Logger &logger = Logger::instance();        \
        logger.setLogLevel(FATAL);                  \
        char buf[1024] = {0};                       \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                            \
        exit(-1);                                   \
    } while(0)

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                \
    do {                                            \
        Logger &logger = Logger::instance();        \
        logger.setLogLevel(DEBUG);                  \
        char buf[1024] = {0};                       \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                            \
    } while(0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

// 类定义
class Logger : noncopyable {
public:
    static Logger& instance();
    void setLogLevel(int level);
    void log(const std::string& msg);  // 改为 const 引用，避免拷贝
private:
    int logLevel_;
    Logger() = default;
};