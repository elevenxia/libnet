#include <sys/epoll.h>
#include <unistd.h>
#include <assert.h>

#include <src/Logger.h>
#include <src/EventLoop.h>

using namespace net;

// int epoll_create(int size);
//  - 创建epollfd，早期linux引入的创建监听epollfd的函数，传入的参数size作为给内核的一个提示
//  - 内核会根据这个size分配一块这么大的数据空间用来监听事件(struct epoll_event)
//  - 当在使用的过程中出现大于size的值时，内核会重新分配内存空间。
//  - 目前这个size已经没有作用，内核可以动态改变数据空间大小，但仍然需要传入大于0的数

// int epoll_create1(int flag);
//  - 创建epollfd，新版linux引入的函数，当flag为0时和不带size的epoll_create效果一样
//  - 目前flag只支持EPOLL_CLOEXEC，在创建的过程中将返回的epollfd描述符设置CLOSE-ON-EXEC属性
//  - 当程序exec执行新程序时自动close epollfd

EPoller::EPoller(EventLoop* loop)
: loop_(loop),
  epollfd_(::epoll_create1(EPOLL_CLOEXEC)), // 将返回的epollfd描述符设置CLOSE-ON-EXEC属性
  events_(1024)
{
  assert(epollfd_ >0);
}

EPoller::~EPoller()
{
  ::close(epollfd_);
}

void EPoller::poll(ChannelList& activeChannels, int timeout)
{
  loop_->assertInLoopThread();

  int maxEvents = static_cast<int>(events_.size());
  int nEvents = ::epoll_wait(epollfd_, events_.data(), maxEvents, timeout);
  if (nEvents == -1) {
    if (errno != EINTR)
      SYSERR("EPoller::epoll_wait()");
  }
  else if (nEvents > 0) {
    // 填充事件
    for (int i = 0; i < nEvents; ++i) {
      Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
      channel->setRevents(events_[i].events); // 产生的事件
      activeChannels.push_back(channel);      // 加入活跃通道
    }
    if (nEvents == maxEvents)
      events_.resize(2 * events_.size());
  }
}

void EPoller::updateChannel(Channel* channel)
{
  loop_->assertInLoopThread();
  /**
   * polling_ 
   *  false ： ADD
   *  true && 没有关注任何事件 ： MOD
   *  true && 关注了事件，DEL
  */
  int op = 0;
  if (!channel->polling()) {
    assert(!channel->isNoneEvents());
    op = EPOLL_CTL_ADD;
    channel->setPollingState(true);
  }
  else if (!channel->isNoneEvents()) {
    op = EPOLL_CTL_MOD;
  }
  else {
    op = EPOLL_CTL_DEL;
    channel->setPollingState(false);
  }

  updateChannel(op, channel);
}

void EPoller::updateChannel(int op, Channel* channel)
{
  struct epoll_event ee;
  ee.events   = channel->events();
  ee.data.ptr = channel;
  int ret = ::epoll_ctl(epollfd_, op, channel->fd(), &ee);
  if (ret == -1)
    SYSERR("EPoller::epoll_ctl()");
}

