#pragma once


/*
	其子被继承后。派生类对象可以正常构造和析构，但派生类对象无法进行拷贝与赋值操作
*/
class noncopyable
{
public:
	noncopyable(const noncopyable&) = delete;
	noncopyable& operator=(const noncopyable&) = delete;
protected:
	noncopyable() = default;
	~noncopyable() = default;

};