// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <xzero-http/Api.h>

namespace xzero {

/**
 * HTTP protocol version number.
 */
enum class HttpVersion {
  UNKNOWN = 0,
  VERSION_0_9 = 0x09,
  VERSION_1_0 = 0x10,
  VERSION_1_1 = 0x11,
  VERSION_2_0 = 0x20,
};

XZERO_HTTP_API const std::string& to_string(HttpVersion value);

} // namespace xzero
