#if defined(_WIN32)

// GetQueuedCompletionStatusEx requires Windows Vista
#define _WIN32_WINNT 0x0600

#include "mongo/platform/windows_basic.h"

#include "mongo/base/descriptor_poller.hpp"

namespace base {

struct DescriptorPoller::InternalPoller {
  static const int MAX_FDS_PER_POLL = 1024;

  HANDLE completionPort_;
  OVERLAPPED_ENTRY events_[MAX_FDS_PER_POLL];

  InternalPoller() : completionPort_(INVALID_HANDLE_VALUE) {}
};

DescriptorPoller::DescriptorPoller() {
  poller_ = new InternalPoller;
}

DescriptorPoller::~DescriptorPoller() {
  if (poller_->completionPort_ != INVALID_HANDLE_VALUE) {
    CloseHandle(poller_->completionPort_);
  }

  delete poller_;
}

void DescriptorPoller::create() {
  HANDLE res = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (res == NULL) {
    perror("Can't create IoCompletionPort");
    exit(1);
  }
  poller_->completionPort_ = res;
}

void DescriptorPoller::setEvent(HANDLE h, Descriptor* descr) {
  if (poller_->completionPort_ == INVALID_HANDLE_VALUE) {
    perror("Can't call setEvent without previous successful create");
    exit(1);
  }
  HANDLE res = CreateIoCompletionPort(h, poller_->completionPort_, reinterpret_cast<size_t>(static_cast<void*>(descr)), 0);
  if (poller_->completionPort_ != res) {
    perror("CreateIoCompletionPort failed to add handle in setEvent");
    exit(1);
  }
}

int DescriptorPoller::poll() {
  ULONG count;
  GetQueuedCompletionStatusEx(poller_->completionPort_,
                     poller_->events_, InternalPoller::MAX_FDS_PER_POLL,
                     &count, /* returned count */
                     100, /* ms */
                     FALSE /* alertable wait */);
  return static_cast<int>(count);
}

// This call would benefit from a returned pointer to an OVERLAPPED structure (i.e. OVERLAPPED**)
void DescriptorPoller::getEvents(int i, int* events, Descriptor** descr) {
  *events &= 0x00000000;
  *descr = reinterpret_cast<Descriptor*>(poller_->events_[i].lpCompletionKey);

  if (poller_->events_[i].lpOverlapped->Internal != ERROR_SUCCESS) {
    *events |= DP_ERROR;
    return;
  }

  // This information is not returned in the completion port; the thread that initiated the
  // overlapped I/O needs to have noted its context either next to the persistent OVERLAPPED
  // structure or in the Descriptor
  //if (poller_->events_[i].events & (EPOLLHUP | EPOLLIN)) {
  //  *events |= DP_READ_READY;
  //}
  //if (poller_->events_[i].events & (EPOLLHUP | EPOLLOUT)) {
  //  *events |= DP_WRITE_READY;
  //}
}

}  // namespace base


#endif  // _WIN32
