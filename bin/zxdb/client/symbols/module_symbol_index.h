// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <map>
#include <memory>
#include <vector>

#include "garnet/bin/zxdb/client/symbols/module_symbol_index_node.h"
#include "garnet/public/lib/fxl/macros.h"
#include "garnet/public/lib/fxl/strings/string_view.h"
#include "llvm/DebugInfo/DWARF/DWARFCompileUnit.h"

namespace llvm {
class DWARFCompileUnit;
class DWARFContext;
class DWARFDie;
}  // namespace llvm

namespace zxdb {

// Holds the index of symbols for a given module.
class ModuleSymbolIndex {
 public:
  using FilePair = std::pair<const std::string, std::vector<llvm::DWARFCompileUnit*>>;

  ModuleSymbolIndex();
  ~ModuleSymbolIndex();

  void CreateIndex(llvm::DWARFContext* context,
                   llvm::DWARFUnitSection<llvm::DWARFCompileUnit>& units);

  const ModuleSymbolIndexNode& root() const { return root_; }

  // Takes a fully-qualified name with namespaces and classes and template
  // parameters and returns the list of symbols which match exactly.
  const std::vector<ModuleSymbolIndexNode::DieRef>& FindFunctionExact(
      const std::string& input) const;

  // Looks up the name in the file index and returns the set of matches. The
  // name is matched from the right side with a left boundary of either a slash
  // or the beginning of the full path. This may match more than one file name,
  // and the caller is left to decide which one(s) it wants.
  //
  // The returned pointers will point into the ModuleSymbolIndex.
  std::vector<const FilePair*> FindFileMatches(const std::string& name) const;

  // Dumps the file index to the stream for debugging.
  void DumpFileIndex(std::ostream& out);

 private:
  void IndexCompileUnit(llvm::DWARFContext* context,
                        llvm::DWARFCompileUnit* unit);

  void IndexCompileUnitSourceFiles(llvm::DWARFContext* context,
                                   llvm::DWARFCompileUnit* unit);

  // Populates the file_name_index_ given a now-unchanging files_ map.
  void IndexFileNames();

  ModuleSymbolIndexNode root_;

  // Maps full path names to compile units that reference them. This must not
  // be mutated once the file_name_index_ is built.
  //
  // This is a map, not a multimap, because some files will appear in many
  // compilation units. I suspect it's better to avoid duplicating the names
  // (like a multimap would) and eating the cost of indirect heap allocations
  // for vectors in the single-item case.
  using FileIndex = std::map<std::string, std::vector<llvm::DWARFCompileUnit*>>;
  FileIndex files_;

  // Maps the last file name component (the part following the last slash) to
  // the set of entries in the files_ index that have that name.
  //
  // This is a multimap because the name parts will generally be unique so we
  // should get few duplicates. The cost of using a vector for most items
  // containing one element becomes higher in that case.
  using FileNameIndex = std::multimap<fxl::StringView, FileIndex::const_iterator>;
  FileNameIndex file_name_index_;

  FXL_DISALLOW_COPY_AND_ASSIGN(ModuleSymbolIndex);
};

}  // namespace zxdb
