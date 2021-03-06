// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero-base/Api.h>
#include <xzero-base/logging/LogLevel.h>

namespace xzero {

class XZERO_API Application {
 public:
  static void init();

  static void logToStderr(LogLevel loglevel = LogLevel::Info);

  static void installGlobalExceptionHandler();
};

} // namespace xzero
