#if defined(_WIN32)

#include "mongo/platform/windows_basic.h"
#include "mongo/base/descriptor_poller.hpp"

#include <vector>

namespace base {

struct DescriptorPoller::InternalPoller {
  std::vector<SOCKET> sockets_;
  std::vector<WSAEVENT> events_;
  std::vector<int> eventFlags_;
  std::vector<Descriptor*> descriptors_;

  InternalPoller() {}
};

DescriptorPoller::DescriptorPoller() {
  poller_ = new InternalPoller;
}

DescriptorPoller::~DescriptorPoller() {
  for (size_t i = 0; i < poller_->sockets_.size(); ++i) {
    CloseHandle(poller_->events_[i]);
  }
  poller_->sockets_.clear();
  poller_->events_.clear();
  poller_->eventFlags_.clear();
  poller_->descriptors_.clear();
  delete poller_;
}

void DescriptorPoller::create() {
}

void DescriptorPoller::setEvent(SOCKET s, Descriptor* descr) {
  WSAEVENT newEvent = WSACreateEvent();
  if (newEvent == WSA_INVALID_EVENT) {
    perror("Can't create WSAEVENT for socket");
    exit(1);
  }
  int status = WSAEventSelect(s, newEvent, FD_READ | FD_WRITE | FD_CLOSE);
  if (status == SOCKET_ERROR) {
    perror("WSAEventSelect failed");
    exit(1);
  }
  poller_->sockets_.push_back(s);
  poller_->events_.push_back(newEvent);
  poller_->descriptors_.push_back(descr);
}

int DescriptorPoller::poll() {
  memset(poller_->eventFlags_.data(), 0, (sizeof(int) * poller_->eventFlags_.size()));
  DWORD res = WSAWaitForMultipleEvents(poller_->events_.size(), 
                                       poller_->events_.data(), 
                                       FALSE,  /* don't wait for all the events */
                                       100,    /* ms */
                                       FALSE); /* do not allow I/O interruptions */

  if (res == WSA_WAIT_TIMEOUT) {
    return 0;
  }
  if (res == WSA_WAIT_FAILED) {
    perror("WSAWaitForMultipleEvents returned WSA_WAIT_FAILED");
    exit(1);
  }
  if (res < WSA_WAIT_EVENT_0 || res >= (WSA_WAIT_EVENT_0 + poller_->events_.size())) {
    perror("WSAWaitForMultipleEvents returned unexpected result");
    exit(1);
  }
  DWORD eventIndex = res - WSA_WAIT_EVENT_0;
  WSANETWORKEVENTS networkEvents;
  int eventCount = 0;
  for (; eventIndex < poller_->events_.size(); ++eventIndex) {
    int status = WSAEnumNetworkEvents(poller_->sockets_[eventIndex], 
                                      poller_->events_[eventIndex],
                                      &networkEvents);
    if (status == SOCKET_ERROR) {
      perror("WSAEnumNetworkEvents failed");
      exit(1);
    }
    bool countThis = false;
    for (int i = 0; i < FD_MAX_EVENTS; ++i) {
      if (networkEvents.iErrorCode[i] != 0) {
        poller_->eventFlags_[eventIndex] |= DP_ERROR;
        countThis = true;
        break;
      }
    }
    if (networkEvents.lNetworkEvents & FD_CLOSE) {
      poller_->eventFlags_[eventIndex] |= DP_ERROR;
      countThis = true;
    }
    if (networkEvents.lNetworkEvents & FD_READ) {
      poller_->eventFlags_[eventIndex] |= DP_READ_READY;
      countThis = true;
    }
    if (networkEvents.lNetworkEvents & FD_WRITE) {
      poller_->eventFlags_[eventIndex] |= DP_WRITE_READY;
      countThis = true;
    }
    if (countThis) {
      ++eventCount;
    }
  }
  return eventCount;
}

void DescriptorPoller::getEvents(int i, int* events, Descriptor** descr) {
  *events = poller_->eventFlags_[i];
  *descr = poller_->descriptors_[i];
}

}  // namespace base

#endif  // _WIN32
