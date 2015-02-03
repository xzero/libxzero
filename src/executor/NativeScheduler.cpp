#include <xzero/executor/NativeScheduler.h>
#include <xzero/RuntimeError.h>
#include <xzero/WallClock.h>
#include <xzero/sysconfig.h>
#include <algorithm>
#include <vector>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>

namespace xzero {

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

/**
 * XXX
 * - registering a key should be *ONESHOT* and *Edge Triggered*
 * - remove SelectionKey::change()
 * - add SelectionKey::cancel()
 * - do we need Selectable then (just use EndPoinit or `int fd`?)
 *   - endpoint: operator int() { return handle(); }
 * - maybe inherit from Executor
 * - add a way to add timed callbacks that can be cancelled
 * - actual implementations inheriting from: Selector < Scheduler < Executor
 * - select: SelectEventLoop (currently: NativeScheduler)
 * - epoll: LinuxEventLoop
 *
 */

NativeScheduler::NativeScheduler(
    std::function<void(const std::exception&)> errorLogger,
    WallClock* clock,
    std::function<void()> preInvoke,
    std::function<void()> postInvoke)
    : Scheduler(std::move(errorLogger)),
      clock_(clock ? clock : WallClock::system()),
      onPreInvokePending_(preInvoke),
      onPostInvokePending_(postInvoke),
      keys_(),
      lock_(),
      wakeupPipe_() {
  if (pipe(wakeupPipe_) < 0) {
    throw SYSTEM_ERROR(errno);
  }
  fcntl(wakeupPipe_[0], F_SETFL, O_NONBLOCK);
  fcntl(wakeupPipe_[1], F_SETFL, O_NONBLOCK);
}

NativeScheduler::NativeScheduler(
    std::function<void(const std::exception&)> errorLogger,
    WallClock* clock)
    : NativeScheduler(errorLogger, clock, nullptr, nullptr) {
}

NativeScheduler::NativeScheduler()
    : NativeScheduler(nullptr, nullptr, nullptr, nullptr) {
}

NativeScheduler::~NativeScheduler() {
  ::close(wakeupPipe_[PIPE_READ_END]);
  ::close(wakeupPipe_[PIPE_WRITE_END]);
}

void NativeScheduler::execute(Task&& task) {
  {
    std::lock_guard<std::mutex> lk(lock_);
    tasks_.push_back(task);
  }
  breakLoop();
}

std::string NativeScheduler::toString() const {
  return "NativeScheduler";
}

Scheduler::HandleRef NativeScheduler::executeAfter(TimeSpan delay, Task task) {
  auto onFire = [task]() {
    task();
  };

  auto onCancel = [this](Handle* handle) {
    removeFromTimersList(handle);
  };

  return insertIntoTimersList(clock_->get() + delay,
                              std::make_shared<Handle>(onFire, onCancel));
}

Scheduler::HandleRef NativeScheduler::executeAt(DateTime when, Task task) {
  auto onFire = [task]() {
    task();
  };

  auto onCancel = [this](Handle* handle) {
    removeFromTimersList(handle);
  };

  return insertIntoTimersList(when, std::make_shared<Handle>(onFire, onCancel));
}

TimeSpan NativeScheduler::computeNextTimeout() {
  std::lock_guard<std::mutex> lk(lock_);

  return timers_.empty()
    ? timers_.front().when - clock_->get()
    : TimeSpan::fromSeconds(4);
}

Scheduler::HandleRef NativeScheduler::insertIntoTimersList(DateTime dt,
                                                           HandleRef handle) {
  Timer t = { dt, handle };

  std::lock_guard<std::mutex> lk(lock_);

  auto i = timers_.end();
  auto e = timers_.begin();

  while (i != e) {
    i--;
    const Timer& current = *i;
    if (current.when >= t.when) {
      timers_.insert(i, t);
      return handle;
    }
  }

  if (i == e) {
    timers_.push_front(t);
  }

  return handle;
}

void NativeScheduler::removeFromTimersList(Handle* handle) {
  std::lock_guard<std::mutex> lk(lock_);

  auto i = timers_.begin();
  auto e = timers_.end();

  while (i != e) {
    if (i->handle.get() == handle) {
      timers_.erase(i);
      break;
    }
    i++;
  }
}

void NativeScheduler::collectTimeouts() {
  const DateTime now = clock_->get();

  std::lock_guard<std::mutex> lk(lock_);

  for (;;) {
    if (timers_.empty())
      break;

    const auto& job = timers_.front();
    if (job.when <= now)
      break;

    tasks_.push_back(std::bind(&Handle::fire, job.handle.get()));
    timers_.pop_front();
  }
}

inline Scheduler::HandleRef registerInterest(
    std::mutex* registryLock,
    std::list<std::pair<int, Scheduler::HandleRef>>* registry,
    int fd,
    Executor::Task task) {

  auto onCancel = [=](Scheduler::Handle* h) {
    std::lock_guard<std::mutex> lk(*registryLock);
    for (auto i: *registry) {
      if (i.second.get() == h) {
        return registry->remove(i);
      }
    }
  };

  std::lock_guard<std::mutex> lk(*registryLock);
  auto handle = std::make_shared<Scheduler::Handle>(task, onCancel);
  registry->push_back(std::make_pair(fd, handle));

  return handle;
}

Scheduler::HandleRef NativeScheduler::executeOnReadable(int fd, Task task) {
  printf("registerInterest(%d, READ)\n", fd);
  return registerInterest(&lock_, &readers_, fd, task);
}

Scheduler::HandleRef NativeScheduler::executeOnWritable(int fd, Task task) {
  printf("registerInterest(%d, WRITE)\n", fd);
  return registerInterest(&lock_, &writers_, fd, task);
}

inline void collectActiveHandles(
    std::list<std::pair<int, Scheduler::HandleRef>>* interests,
    fd_set* fdset,
    std::vector<Scheduler::HandleRef>* result) {

  auto i = interests->begin();
  auto e = interests->end();

  while (i != e) {
    if (FD_ISSET(i->first, fdset)) {
      result->push_back(i->second);
      auto k = i;
      ++i;
      interests->erase(k);
    } else {
      i++;
    }
  }
}

void NativeScheduler::runLoopOnce() {
  fd_set input, output, error;
  FD_ZERO(&input);
  FD_ZERO(&output);
  FD_ZERO(&error);

  int wmark = 0;
  timeval tv;

  {
    std::lock_guard<std::mutex> lk(lock_);

    for (auto i: readers_) {
      FD_SET(i.first, &input);
      if (i.first > wmark) {
        wmark = i.first;
      }
    }

    for (auto i: writers_) {
      FD_SET(i.first, &output);
      if (i.first > wmark) {
        wmark = i.first;
      }
    }

    const TimeSpan nextTimeout = !tasks_.empty()
                               ? TimeSpan::Zero
                               : !timers_.empty()
                                 ? timers_.front().when - clock_->get()
                                 : TimeSpan::fromSeconds(4);

    tv.tv_sec = static_cast<time_t>(nextTimeout.totalSeconds()),
    tv.tv_usec = nextTimeout.microseconds();
  }

  FD_SET(wakeupPipe_[PIPE_READ_END], &input);

  int rv = ::select(wmark + 1, &input, &output, &error, &tv);
  if (rv < 0)
    throw SYSTEM_ERROR(errno);

  collectTimeouts();

  if (FD_ISSET(wakeupPipe_[PIPE_READ_END], &input)) {
    bool consumeMore = true;
    while (consumeMore) {
      char buf[sizeof(int) * 128];
      consumeMore = ::read(wakeupPipe_[PIPE_READ_END], buf, sizeof(buf)) > 0;
    }
  }

  std::vector<HandleRef> activeHandles;
  activeHandles.reserve(rv);

  std::deque<Task> activeTasks;
  {
    std::lock_guard<std::mutex> lk(lock_);

    collectActiveHandles(&readers_, &input, &activeHandles);
    collectActiveHandles(&writers_, &output, &activeHandles);
    activeTasks = std::move(tasks_);
  }

  safeCall(onPreInvokePending_);
  safeCallEach(activeHandles);
  safeCallEach(activeTasks);
  safeCall(onPostInvokePending_);
}

void NativeScheduler::breakLoop() {
  int dummy = 42;
  ::write(wakeupPipe_[PIPE_WRITE_END], &dummy, sizeof(dummy));
}

} // namespace xzero
