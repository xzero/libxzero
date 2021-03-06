// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Api.h>
#include <xzero-flow/vm/ConstantPool.h>
#include <xzero-flow/vm/Instruction.h>
#include <xzero-flow/FlowType.h>  // FlowNumber
#include <xzero-base/net/IPAddress.h>
#include <xzero-base/net/Cidr.h>
#include <xzero-base/RegExp.h>

#include <vector>
#include <utility>
#include <memory>

namespace xzero {
namespace flow {
namespace vm {

class Runner;
class Runtime;
class Handler;
class Match;
class MatchDef;
class NativeCallback;
class ConstantPool;

class XZERO_FLOW_API Program {
 public:
  explicit Program(ConstantPool&& cp);
  Program(Program&) = delete;
  Program& operator=(Program&) = delete;
  ~Program();

  const ConstantPool& constants() const { return cp_; }

  // accessors to linked data
  const Match* match(size_t index) const { return matches_[index]; }
  Handler* handler(size_t index) const { return handlers_[index]; }
  NativeCallback* nativeHandler(size_t index) const {
    return nativeHandlers_[index];
  }
  NativeCallback* nativeFunction(size_t index) const {
    return nativeFunctions_[index];
  }

  // bulk accessors
  const std::vector<Match*>& matches() const { return matches_; }
  inline const std::vector<Handler*> handlers() const { return handlers_; }

  int indexOf(const Handler* handler) const;
  Handler* findHandler(const std::string& name) const;

  bool link(Runtime* runtime);

  void dump();

 private:
  // builders
  Handler* createHandler(const std::string& name);
  Handler* createHandler(const std::string& name,
                         const std::vector<Instruction>& instructions);

 private:
  void setup(const std::vector<MatchDef>& matches);

 private:
  ConstantPool cp_;

  // linked data
  Runtime* runtime_;
  std::vector<Handler*> handlers_;
  std::vector<Match*> matches_;
  std::vector<NativeCallback*> nativeHandlers_;
  std::vector<NativeCallback*> nativeFunctions_;
};

}  // namespace vm
}  // namespace flow
}  // namespace xzero
