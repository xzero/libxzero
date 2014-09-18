#include <xzero/support/libev/LibevSelector.h>
#include <xzero/io/SelectionKey.h>
#include <cassert>
#include <mutex>
#include <ev++.h>

namespace xzero {
namespace support {

#if 0 // !defined(NDEBUG)
static std::mutex m;
#define TRACE(msg...)  do { \
    m.lock(); \
    printf("LibevSelector: " msg); \
    printf("\n"); \
    m.unlock(); \
  } while (0);
#else
#define TRACE(msg...) do { } while (0)
#endif

/**
 * Wrapper around @c ev::io.
 */
class LibevIO : public SelectionKey { // {{{
 public:
  LibevIO(LibevSelector* selector, Selectable* selectable, unsigned ops)
      : SelectionKey(selector),
        io_(selector->loop()),
        selectable_(selectable) {

    assert(selector != nullptr);
    assert(selectable != nullptr);
    assert(selectable->handle() >= 0);
    assert((ops & Selectable::READ) || (ops & Selectable::WRITE));

    TRACE("LibevIO: fd=%d, ops=%u", selectable->handle(), ops);

    io_.set<LibevIO, &LibevIO::io>(this);
    change(ops);

    io_.start();
  }

  ~LibevIO() {
    TRACE("~LibevIO: fd=%d", io_.fd);
  }

  void change(int ops) override {
    unsigned flags = 0;
    if (ops & Selectable::READ)
      flags |= ev::READ;

    if (ops & Selectable::WRITE)
      flags |= ev::WRITE;

    TRACE("LibevIO.change: fd=%d, ops=%d", selectable_->handle(), ops);

    io_.set(selectable_->handle(), flags);
  }

  void io(ev::io&, int revents) {
    unsigned flags = 0;

    if (revents & ev::READ)
      flags |= Selectable::READ;

    if (revents & ev::WRITE)
      flags |= Selectable::WRITE;

    setActivity(flags);

    selectable_->onSelectable();
  }

 private:
  ev::io io_;
  Selectable* selectable_;
};
// }}}

LibevSelector::LibevSelector(ev::loop_ref loop)
    : loop_(loop),
      evWakeup_(loop_) {
  evWakeup_.set<LibevSelector, &LibevSelector::onWakeup>(this);
  evWakeup_.start();
}

LibevSelector::~LibevSelector() {
}

void LibevSelector::onWakeup(ev::async&, int) {
  TRACE("onWakeup!");
  loop_.break_loop(ev::ALL);
}

std::unique_ptr<SelectionKey> LibevSelector::createSelectable(
    Selectable* handle, unsigned ops) {
  return std::unique_ptr<SelectionKey>(new LibevIO(this, handle, ops));
}

void LibevSelector::select() {
  loop_.run();
  TRACE("select: leaving");
}

void LibevSelector::wakeup() {
  TRACE("wakeup");
  evWakeup_.send();
  loop_.break_loop(ev::ALL);
}

} // namespace support
} // namespace xzero