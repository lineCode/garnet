// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/fxl/files/file_descriptor.h"
#include "lib/fxl/portable_unistd.h"

#include <fcntl.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "lib/fxl/files/scoped_temp_dir.h"
#include "lib/fxl/files/unique_fd.h"

namespace fxl {
namespace {

TEST(FileDescriptor, WriteAndRead) {
  files::ScopedTempDir temp_dir;
  std::string path;
  ASSERT_TRUE(temp_dir.NewTempFile(&path));

  fxl::UniqueFD fd(open(path.c_str(), O_RDWR));
  ASSERT_TRUE(fd.is_valid());

  std::string string = "one, two, three";
  EXPECT_TRUE(WriteFileDescriptor(fd.get(), string.data(), string.size()));
  EXPECT_EQ(0, lseek(fd.get(), 0, SEEK_SET));

  std::vector<char> buffer;
  buffer.resize(1024);
  ssize_t read = ReadFileDescriptor(fd.get(), buffer.data(), 1024);
  EXPECT_EQ(static_cast<ssize_t>(string.size()), read);
  EXPECT_EQ(string, buffer.data());
}

}  // namespace
}  // namespace fxl
