#pragma once

#include <xzero/Api.h>
#include <string>

namespace xzero {

/**
 * Represents a single HTTP message header name/value pair.
 */
class XZERO_API HeaderField {
 public:
  HeaderField() = default;
  HeaderField(HeaderField&&) = default;
  HeaderField(const HeaderField&) = default;
  HeaderField& operator=(HeaderField&&) = default;
  HeaderField& operator=(const HeaderField&) = default;
  HeaderField(const std::string& name, const std::string& value);

  const std::string& name() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  const std::string& value() const { return value_; }
  void setValue(const std::string& value) { value_ = value; }

  /** Performs an case-insensitive compare on name and value for equality. */
  bool operator==(const HeaderField& other) const;

  /** Performs an case-insensitive compare on name and value for inequality. */
  bool operator!=(const HeaderField& other) const;

 private:
  std::string name_;
  std::string value_;
};

}  // namespace xzero