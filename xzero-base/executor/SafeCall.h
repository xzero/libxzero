// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-base/Api.h>
#include <xzero-base/sysconfig.h>

#include <exception>
#include <deque>
#include <functional>
#include <string>

namespace xzero {

/**
 * helper class for safely invoking callbacks with regards to unhandled
 * exceptions.
 *
 * Unhandled exceptions will be caught and passed to the exception handler,
 * i.e. to log them.
 */
class XZERO_API SafeCall {
 public:
  SafeCall();
  explicit SafeCall(std::function<void(const std::exception&)> eh);

  /**
   * Configures exception handler.
   */
  void setExceptionHandler(std::function<void(const std::exception&)> eh);

  /**
   * Savely invokes given task within the callers context.
   *
   * A call to this function will never leak an unhandled exception.
   *
   * @see setExceptionHandler(std::function<void(const std::exception&)>)
   */
  void safeCall(std::function<void()> callee) XZERO_NOEXCEPT;

  /**
   * Convinience call operator.
   *
   * @see void safeCall(std::function<void()> callee)
   */
  void operator()(std::function<void()> callee) XZERO_NOEXCEPT {
    safeCall(callee);
  }

 protected:
  /**
   * Handles uncaught exception.
   */
  void handleException(const std::exception& e) XZERO_NOEXCEPT;

 private:
  std::function<void(const std::exception&)> exceptionHandler_;
};

} // namespace xzero