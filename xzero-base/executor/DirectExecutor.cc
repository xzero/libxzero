// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero-base/executor/DirectExecutor.h>
#include <xzero-base/logging.h>
#include <stdio.h>

namespace xzero {

#ifndef NDEBUG
#define TRACE(msg...) logTrace("executor.DirectExecutor", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

DirectExecutor::DirectExecutor(
    bool recursive,
    std::function<void(const std::exception&)> eh)
    : Executor(std::move(eh)),
      recursive_(recursive),
      running_(0),
      deferred_() {
}

void DirectExecutor::execute(Task task) {
  if (isRunning() && !isRecursive()) {
    deferred_.push_back(std::move(task));
    TRACE("%p execute: enqueue task (%zu)", this, deferred_.size());
    return;
  }

  running_++;

  TRACE("%p execute: run top-level task", this);
  safeCall(task);

  while (!deferred_.empty()) {
    TRACE("%p execute: run deferred task (-%zu)", this, deferred_.size());
    safeCall(deferred_.front());
    deferred_.pop_front();
  }

  running_--;
}

std::string DirectExecutor::toString() const {
  char buf[128];
  snprintf(buf, sizeof(buf), "DirectExecutor@%p", this);
  return buf;
}

} // namespace xzero
