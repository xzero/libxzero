// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-base/Api.h>
#include <xzero-base/sysconfig.h>
#include <xzero-base/executor/Executor.h>
#include <deque>

namespace xzero {

/**
 * Executor to directly invoke the tasks being passed by the caller.
 *
 * @note Not thread-safe.
 */
class XZERO_API DirectExecutor : public Executor {
 public:
  DirectExecutor(
    bool recursive = false,
    std::function<void(const std::exception&)> eh = nullptr);

  void execute(Task task) override;
  std::string toString() const override;

  /** Tests whether this executor is currently running some task. */
  bool isRunning() const { return running_ > 0; }

  /** Tests whether this executor allows recursive execution of tasks. */
  bool isRecursive() const { return recursive_; }

  /** Sets wether recursive execution is allowed or flattened. */
  void setRecursive(bool value) { recursive_ = value; }

  /** Retrieves number of deferred tasks. */
  size_t backlog() const { return deferred_.size(); }

 private:
  bool recursive_;
  int running_;
  std::deque<Task> deferred_;
};

} // namespace xzero