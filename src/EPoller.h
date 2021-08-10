#pragma once 

#include <vector>

#include <src/noncopyable.h>

namespace net
{
// 类中采用前向声明，即在定义Poller之前声明一下class Channel;
// 用处是避免让头文件#include <muduo/net/Channel.h>从而增加依赖性，
// 因为头文件中并没有使用Channel，只是定义了这个类型的变量，所以只声明就好了，
// 而在成员函数的实现中需要使用Channel的接口，这就需要让编译器知道Channel是怎么定义的，
// 就需要在.cpp文件中#include <muduo/net/Channel.h>。
// 另外，因为Channel是Poller的成员变量，当Poller析构时也会调用Channel的析构函数，
// 这就需要让编译器知道Channel析构函数的定义，所以Poller的析构函数需要在.cpp中定义。
class EventLoop;
class Channel;

class EPoller: noncopyable
{
public:
  using ChannelList = std::vector<Channel*>;
  using EventList   = std::vector<struct epoll_event>;

  explicit
  EPoller(EventLoop* loop);
  ~EPoller();

  void poll(ChannelList& activeChannels,int timeout=-1); // epoll_wait
  void updateChannel(Channel* channel); // 更新监听事件，增删改对fd的监听事件

private:
  void updateChannel(int op, Channel* channel);

  EventLoop* loop_;
  int        epollfd_;
  EventList  events_; // 产生的事件
};
}