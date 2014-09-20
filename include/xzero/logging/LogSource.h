#pragma once

#include <xzero/Api.h>
#include <string>

namespace xzero {

/**
 * A logging source.
 *
 * Creates a log message such as "[$ClassName] $LogMessage"
 *
 * Your class that needs logging creates a static member of LogSource
 * for generating logging messages.
 */
class XZERO_API LogSource {
 public:
  explicit LogSource(const std::string& className);
  ~LogSource();

  void trace(const char* fmt, ...);
  void debug(const char* fmt, ...);
  void info(const char* fmt, ...);
  void warn(const char* fmt, ...);
  void error(const char* fmt, ...);

  void enable();
  bool isEnabled() const noexcept;
  void disable();

  const std::string& className() const noexcept { return className_; }

 private:
  std::string className_;
  bool enabled_;
};

}  // namespace xzero