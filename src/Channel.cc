#include <assert.h>

#include <src/EventLoop.h>
#include <src/Channel.h>

using namespace net;

Channel::Channel(EventLoop* loop, int fd)
: polling_(false),
  tied_(false),
  handlingEvents_(false),
  fd_(fd),
  events_(0),
  revents_(0),
  loop_(loop),
  readCallback_(nullptr),
  writeCallback_(nullptr),
  closeCallback_(nullptr),
  errorCallback_(nullptr)
{ }

Channel::~Channel()
{ assert(!handlingEvents_); }

void Channel::handleEvents()
{
  loop_->assertInLoopThread();
  // 处理事件
  if (tied_) 
  {
    auto guard = tie_.lock();
    /// 这个 @b guard 作用
    /// 1 查看 TcpConnection 对象是否存在，如果不存在则guard为nullptr
    /// 2 如果 TcpConnection 对象还没有断开连接，那么这个 guard 可以保证在处理事件过程中不会被释放
    ///   延长其生命周期
    if (guard != nullptr)
      handleEventsWithGuard();
  }
  else
  {
    // 这里实际上不会执行
    handleEventsWithGuard();
  } 
}

// handleEventsWithGuard根据不同的激活原因用不同的回调函数，这些回调函数都在TcpConnection中，
// 所以在调用前必须判断Channel所在的TcpConnection是否还存在！
void Channel::handleEventsWithGuard()
{
  handlingEvents_ = true;

  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    if (closeCallback_) 
      closeCallback_();
  }
  if (revents_ & EPOLLERR) {
    if (errorCallback_) 
      errorCallback_();
  }
  if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
    if (readCallback_) 
      readCallback_();
  }
  if (revents_ & EPOLLOUT) {
    if (writeCallback_) 
      writeCallback_();
  }

  handlingEvents_ = false;
}

// 由TcpConnection中connectEstablished函数调用
// 作用是当建立起一个Tcp连接后，让用于监测fd的Channel保存这个Tcp连接的弱引用
// tie_是weak_ptr的原因：
// weak_ptr是弱引用，不增加引用基数，当需要调用内部指针时
// 可通过lock函数提升成一个shared_ptr，如果内部的TcpConnection指针已经销毁
// 那么提升的shared_ptr是nullptr
// 如果 TcpConnection 对象还没有断开连接，那么这个 guard 可以保证在处理事件过程中不会被释放
void Channel::tie(const std::shared_ptr<void>& obj)
{
  tie_ = obj;
  tied_ = true;
}

void Channel::update()
{
  loop_->updateChannel(this);
}

void Channel::remove()
{
  assert(polling_);
  loop_->removeChannel(this);
}
