// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero-base/executor/Scheduler.h>
#include <xzero-base/net/InetConnector.h>
#include <xzero-base/net/InetEndPoint.h>
#include <xzero-base/net/ConnectionFactory.h>
#include <xzero-base/net/Connection.h>
#include <xzero-base/net/IPAddress.h>
#include <xzero-base/RuntimeError.h>
#include <xzero-base/logging.h>
#include <xzero-base/sysconfig.h>
#include <algorithm>
#include <mutex>
#include <system_error>

#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#if defined(SD_FOUND)
#include <systemd/sd-daemon.h>
#endif

#if !defined(SO_REUSEPORT)
#define SO_REUSEPORT 15
#endif

#if 0 // !defined(NDEBUG)
static std::mutex m;
#define TRACE(msg...)  do { \
    m.lock(); \
    printf("InetConnector: " msg); \
    printf("\n"); \
    m.unlock(); \
  } while (0);
#else
#define TRACE(msg...) do { } while (0)
#endif

namespace xzero {

InetConnector::InetConnector(const std::string& name, Executor* executor,
                             Scheduler* scheduler, WallClock* clock,
                             TimeSpan idleTimeout,
                             TimeSpan tcpFinTimeout,
                             std::function<void(const std::exception&)> eh)
    : Connector(name, executor, clock),
      scheduler_(scheduler),
      schedulerHandle_(),
      safeCall_(eh),
      connectedEndPoints_(),
      mutex_(),
      socket_(-1),
      addressFamily_(IPAddress::V4),
      typeMask_(0),
      flags_(0),
      blocking_(true),
      backlog_(256),
      multiAcceptCount_(1),
      idleTimeout_(idleTimeout),
      tcpFinTimeout_(tcpFinTimeout),
      isStarted_(false) {
}

InetConnector::InetConnector(const std::string& name, Executor* executor,
                             Scheduler* scheduler, WallClock* clock,
                             TimeSpan idleTimeout,
                             TimeSpan tcpFinTimeout,
                             std::function<void(const std::exception&)> eh,
                             const IPAddress& ipaddress, int port, int backlog,
                             bool reuseAddr, bool reusePort)
    : InetConnector(name, executor, scheduler, clock,
                    idleTimeout, tcpFinTimeout, eh) {

  open(ipaddress, port, backlog, reuseAddr, reusePort);
}

void InetConnector::open(const IPAddress& ipaddress, int port, int backlog,
                         bool reuseAddr, bool reusePort) {
  if (isOpen())
    RAISE_STATUS(IllegalStateError);

  socket_ = ::socket(ipaddress.family(), SOCK_STREAM, 0);
  addressFamily_ = ipaddress.family();

  if (socket_ < 0)
    RAISE_ERRNO(errno);

  if (reusePort)
    setReusePort(reusePort);

  if (reuseAddr)
    setReuseAddr(reuseAddr);

  bind(ipaddress, port);
}

void InetConnector::close() {
  if (isStarted()) {
    stop();
  }

  if (isOpen()) {
    ::close(socket_);
    socket_ = -1;
  }
}

void InetConnector::bind(const IPAddress& ipaddr, int port) {
  char sa[sizeof(sockaddr_in6)];
  socklen_t salen = ipaddr.size();

  switch (ipaddr.family()) {
    case IPAddress::V4:
      salen = sizeof(sockaddr_in);
      ((sockaddr_in*)sa)->sin_port = htons(port);
      ((sockaddr_in*)sa)->sin_family = AF_INET;
      memcpy(&((sockaddr_in*)sa)->sin_addr, ipaddr.data(), ipaddr.size());
      break;
    case IPAddress::V6:
      salen = sizeof(sockaddr_in6);
      ((sockaddr_in6*)sa)->sin6_port = htons(port);
      ((sockaddr_in6*)sa)->sin6_family = AF_INET6;
      memcpy(&((sockaddr_in6*)sa)->sin6_addr, ipaddr.data(), ipaddr.size());
      break;
    default:
      RAISE_ERRNO(EINVAL);
  }

  int rv = ::bind(socket_, (sockaddr*)sa, salen);
  if (rv < 0)
    RAISE_ERRNO(errno);

  addressFamily_ = ipaddr.family();
}

void InetConnector::listen(int backlog) {
  int rv = ::listen(socket_, backlog);
  if (rv < 0)
    RAISE_ERRNO(errno);
}

bool InetConnector::isOpen() const XZERO_NOEXCEPT {
  return socket_ >= 0;
}

InetConnector::~InetConnector() {
  TRACE("~InetConnector");
  if (isStarted()) {
    stop();
  }

  if (socket_ >= 0) {
    ::close(socket_);
  }
}

int InetConnector::handle() const XZERO_NOEXCEPT {
  return socket_;
}

void InetConnector::setSocket(int socket) {
  socket_ = socket;
}

size_t InetConnector::backlog() const XZERO_NOEXCEPT {
  return backlog_;
}

void InetConnector::setBacklog(size_t value) {
  if (isStarted()) {
    RAISE_STATUS(IllegalStateError);
  }

  backlog_ = value;
}

bool InetConnector::isBlocking() const {
  return !(fcntl(socket_, F_GETFL) & O_NONBLOCK);
}

void InetConnector::setBlocking(bool enable) {
  unsigned flags = enable ? fcntl(socket_, F_GETFL) & ~O_NONBLOCK
                          : fcntl(socket_, F_GETFL) | O_NONBLOCK;

  if (fcntl(socket_, F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }

#if defined(HAVE_ACCEPT4) && defined(ENABLE_ACCEPT4)
  if (enable) {
    typeMask_ &= ~SOCK_NONBLOCK;
  } else {
    typeMask_ |= SOCK_NONBLOCK;
  }
#else
  if (enable) {
    flags_ &= ~O_NONBLOCK;
  } else {
    flags_ |= O_NONBLOCK;
  }
#endif
  blocking_ = enable;
}

bool InetConnector::closeOnExec() const {
  return fcntl(socket_, F_GETFD) & FD_CLOEXEC;
}

void InetConnector::setCloseOnExec(bool enable) {
  unsigned flags = enable ? fcntl(socket_, F_GETFD) | FD_CLOEXEC
                          : fcntl(socket_, F_GETFD) & ~FD_CLOEXEC;

  if (fcntl(socket_, F_SETFD, flags) < 0) {
    RAISE_ERRNO(errno);
  }

#if defined(HAVE_ACCEPT4) && defined(ENABLE_ACCEPT4)
  if (enable) {
    typeMask_ |= SOCK_CLOEXEC;
  } else {
    typeMask_ &= ~SOCK_CLOEXEC;
  }
#else
  if (enable) {
    flags_ |= O_CLOEXEC;
  } else {
    flags_ &= ~O_CLOEXEC;
  }
#endif
}

bool InetConnector::deferAccept() const {
#if defined(TCP_DEFER_ACCEPT)
  int optval = 1;
  socklen_t optlen = sizeof(optval);
  return ::getsockopt(socket_, SOL_TCP, TCP_DEFER_ACCEPT, &optval, &optlen) == 0
             ? optval != 0
             : false;
#else
  return false;
#endif
}

void InetConnector::setDeferAccept(bool enable) {
#if defined(TCP_DEFER_ACCEPT)
  int rc = enable ? 1 : 0;
  if (::setsockopt(socket_, SOL_TCP, TCP_DEFER_ACCEPT, &rc, sizeof(rc)) < 0) {
    RAISE_ERRNO(errno);
  }
#elif defined(SO_ACCEPTFILTER)
  // XXX this compiles on FreeBSD but not on OSX (why don't they support it?)
  accept_filter_arg afa;
  strcpy(afa.af_name, "dataready");
  afa.af_arg[0] = 0;

  if (setsockopt(serverfd, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof(afa)) < 0) {
    RAISE_ERRNO(errno);
  }
#else
  if (enable) {
    logWarning("InetConnector",
               "Ignoring setting TCP_DEFER_ACCEPT. Not supported.");
  }
#endif
}

bool InetConnector::quickAck() const {
#if defined(TCP_QUICKACK)
  int optval = 1;
  socklen_t optlen = sizeof(optval);
  return ::getsockopt(socket_, SOL_TCP, TCP_QUICKACK, &optval, &optlen) == 0
             ? optval != 0
             : false;
#else
  return false;
#endif
}

void InetConnector::setQuickAck(bool enable) {
#if defined(TCP_QUICKACK)
  int rc = enable ? 1 : 0;
  if (::setsockopt(socket_, SOL_TCP, TCP_QUICKACK, &rc, sizeof(rc)) < 0) {
    RAISE_ERRNO(errno);
  }
#else
  // ignore
#endif
}

bool InetConnector::reusePort() const {
  int optval = 1;
  socklen_t optlen = sizeof(optval);
  return ::getsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, &optval, &optlen) == 0
             ? optval != 0
             : false;
}

void InetConnector::setReusePort(bool enable) {
  int rc = enable ? 1 : 0;
  if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, &rc, sizeof(rc)) < 0) {
    RAISE_ERRNO(errno);
  }
}

bool InetConnector::reuseAddr() const {
  int optval = 1;
  socklen_t optlen = sizeof(optval);
  return ::getsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen) == 0
             ? optval != 0
             : false;
}

void InetConnector::setReuseAddr(bool enable) {
  int rc = enable ? 1 : 0;
  if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &rc, sizeof(rc)) < 0) {
    RAISE_ERRNO(errno);
  }
}

size_t InetConnector::multiAcceptCount() const XZERO_NOEXCEPT {
  return multiAcceptCount_;
}

void InetConnector::setMultiAcceptCount(size_t value) XZERO_NOEXCEPT {
  multiAcceptCount_ = value;
}

void InetConnector::setIdleTimeout(TimeSpan value) {
  idleTimeout_ = value;
}

void InetConnector::setTcpFinTimeout(TimeSpan value) {
  tcpFinTimeout_ = value;
}

void InetConnector::start() {
  if (!isOpen()) {
    RAISE_STATUS(IllegalStateError);
  }

  if (isStarted()) {
    return;
  }

  listen(backlog_);

  isStarted_ = true;

  notifyOnEvent();
}

void InetConnector::notifyOnEvent() {
  schedulerHandle_ = scheduler_->executeOnReadable(
      handle(),
      std::bind(&InetConnector::onConnect, this));
}

bool InetConnector::isStarted() const XZERO_NOEXCEPT {
  return isStarted_;
}

void InetConnector::stop() {
  TRACE("stop: %s", name().c_str());
  if (!isStarted()) {
    return;
  }

  if (schedulerHandle_)
    schedulerHandle_->cancel();

  isStarted_ = false;
}

void InetConnector::onConnect() {
  safeCall_([this]() {
    for (size_t i = 0; i < multiAcceptCount_; i++) {
      int cfd = acceptOne();
      if (cfd < 0)
        break;

      RefPtr<EndPoint> endpoint = createEndPoint(cfd);
      {
        std::lock_guard<std::mutex> _lk(mutex_);
        connectedEndPoints_.push_back(endpoint);
      }

      endpoint->setIdleTimeout(idleTimeout_);

      onEndPointCreated(endpoint);
    }
  });

  if (isStarted()) {
    notifyOnEvent();
  }
}

int InetConnector::acceptOne() {
#if defined(HAVE_ACCEPT4) && defined(ENABLE_ACCEPT4)
  bool flagged = true;
  int cfd = ::accept4(socket_, nullptr, 0, typeMask_);
  if (cfd < 0 && errno == ENOSYS) {
    cfd = ::accept(socket_, nullptr, 0);
    flagged = false;
  }
#else
  bool flagged = false;
  int cfd = ::accept(socket_, nullptr, 0);
#endif

  if (cfd < 0) {
    switch (errno) {
      case EINTR:
      case EAGAIN:
#if EAGAIN != EWOULDBLOCK
      case EWOULDBLOCK:
#endif
        return -1;
      default:
        RAISE_ERRNO(errno);
    }
  }

  if (!flagged && flags_ &&
        fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL) | flags_) < 0) {
    ::close(cfd);
    RAISE_ERRNO(errno);
  }

#if defined(TCP_LINGER2)
  if (tcpFinTimeout_ != TimeSpan::Zero) {
    int waitTime = tcpFinTimeout_.totalSeconds();
    int rv = setsockopt(cfd, SOL_TCP, TCP_LINGER2, &waitTime, sizeof(waitTime));
    if (rv < 0) {
      ::close(cfd);
      RAISE_ERRNO(errno);
    }
  }
#endif

  return cfd;
}

RefPtr<EndPoint> InetConnector::createEndPoint(int cfd) {
  return make_ref<InetEndPoint>(cfd, this, scheduler_).as<EndPoint>();
}

void InetConnector::onEndPointCreated(const RefPtr<EndPoint>& endpoint) {
  // create Connection object for given endpoint
  defaultConnectionFactory()->create(this, endpoint.get());

  safeCall_(std::bind(&Connection::onOpen, endpoint->connection()));
}

std::list<RefPtr<EndPoint>> InetConnector::connectedEndPoints() {
  std::list<RefPtr<EndPoint>> result;
  std::lock_guard<std::mutex> _lk(mutex_);
  for (const RefPtr<EndPoint>& ep : connectedEndPoints_) {
    result.push_back(ep);
  }
  return result;
}

void InetConnector::onEndPointClosed(EndPoint* endpoint) {
  assert(endpoint != nullptr);
  assert(endpoint->connection() != nullptr);

  std::lock_guard<std::mutex> _lk(mutex_);

  auto i = std::find(connectedEndPoints_.begin(), connectedEndPoints_.end(),
                     endpoint);

  assert(i != connectedEndPoints_.end());

  if (i != connectedEndPoints_.end()) {
    safeCall_(std::bind(&Connection::onClose, endpoint->connection()));

    connectedEndPoints_.erase(i);
  }
}

}  // namespace xzero
