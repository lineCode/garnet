// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/zxdb/client/symbols/mock_module_symbols.h"

#include "garnet/bin/zxdb/client/symbols/location.h"

namespace zxdb {

MockModuleSymbols::MockModuleSymbols(const std::string& local_file_name)
    : local_file_name_(local_file_name) {}
MockModuleSymbols::~MockModuleSymbols() = default;

void MockModuleSymbols::AddSymbol(const std::string& name,
                                  std::vector<uint64_t> addrs) {
  symbols_[name] = std::move(addrs);
}

const std::string& MockModuleSymbols::GetLocalFileName() const {
  return local_file_name_;
}

Location MockModuleSymbols::RelativeLocationForRelativeAddress(
    uint64_t address) const {
  // Currently only name -> address mappings are supported by this mock.
  return Location(Location::State::kAddress, address);
}

std::vector<uint64_t> MockModuleSymbols::RelativeAddressesForFunction(
    const std::string& name) const {
  auto found = symbols_.find(name);
  if (found == symbols_.end())
    return std::vector<uint64_t>();
  return found->second;
}

}  // namespace zxdb
