#pragma once 

#include <functional>
#include <memory>

#include <sys/epoll.h>

#include <src/noncopyable.h>

namespace net
{

class EventLoop;

// Channel其实就是一个事件类，保存fd和需要监听的事件，以及各种回调函数
class Channel: noncopyable {
public:
  using ReadCallback  = std::function<void()> ; // 使用C++11的 using 代替 typedef
  using WriteCallback = std::function<void()> ;
  using CloseCallback = std::function<void()> ;
  using ErrorCallback = std::function<void()> ;

  Channel(EventLoop* loop, int fd);
  ~Channel();

  void setReadCallback (const ReadCallback& cb) { readCallback_  = cb; }
  void setWriteCallback(const WriteCallback& cb){ writeCallback_ = cb; }
  void setCloseCallback(const CloseCallback& cb){ closeCallback_ = cb; }
  void setErrorCallback(const ErrorCallback& cb){ errorCallback_ = cb; }

  void setRevents(uint32_t revents) { revents_ = revents; }
  void setPollingState(bool state)  { polling_ = state;   }

  int      fd()           const { return fd_;                }
  uint32_t events()       const { return events_;            }
  bool     polling()      const { return polling_;           }
  bool     isNoneEvents() const { return events_ == 0;       }
  bool     isReading()    const { return events_ & EPOLLIN;  }
  bool     isWriting()    const { return events_ & EPOLLOUT; }

  // 将自己注册到Poller中，或从Poller中移除，或只删除某个事件，update进行更新设置
  void enableRead()  { events_ |= EPOLLIN | EPOLLPRI;  update();}
  void enableWrite() { events_ |= EPOLLOUT;            update();}
  void disableRead() { events_ &= ~EPOLLIN;            update();}
  void disableWrite(){ events_ &= ~EPOLLOUT;           update();}
  void disableAll()  { events_ = 0;                    update();}

  void remove();
  void tie(const std::shared_ptr<void>& obj); // 用于保存TcpConnection指针
  void handleEvents();
private:
  void update();
  void handleEventsWithGuard();

  bool                polling_;
  bool                tied_;
  bool                handlingEvents_; 
  int                 fd_;
  uint32_t            events_;          // 关注的事件
  uint32_t            revents_;         // 产生的事件
  EventLoop*          loop_;
  // tie_存储的是TcpConnection类型的指针，即TcpConnectionPtr
  std::weak_ptr<void> tie_;             // 延长TcpConnection的生命周期
  ReadCallback        readCallback_;
  WriteCallback       writeCallback_;
  CloseCallback       closeCallback_;
  ErrorCallback       errorCallback_;
};

}
