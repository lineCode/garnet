// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>

#include <fuchsia/cpp/media.h>
#include <lib/async/cpp/auto_wait.h>
#include <lib/zx/socket.h>

#include "lib/fidl/cpp/binding.h"
#include "lib/fxl/logging.h"

namespace media {

// Fake SeekingReader that 'reads' a synthetic wav file.
class FakeWavReader : public SeekingReader {
 public:
  // Constructs a FakeWavReader that produces a file of |size| bytes total.
  FakeWavReader();

  ~FakeWavReader() override;

  void SetSize(uint64_t size) {
    FXL_DCHECK(size > kMasterChunkHeaderSize + kFormatChunkSize +
                          kDataChunkHeaderSize);
    size_ = size;
    WriteHeader();
  }

  // Binds the reader.
  void Bind(fidl::InterfaceRequest<SeekingReader> request);

  // SeekingReader implementation.
  void Describe(DescribeCallback callback) override;

  void ReadAt(uint64_t position, ReadAtCallback callback) override;

 private:
  static constexpr size_t kMasterChunkHeaderSize = 12;
  static constexpr size_t kFormatChunkSize = 24;
  static constexpr size_t kDataChunkHeaderSize = 8;
  static constexpr size_t kChunkSizeDeficit = 8;

  static constexpr uint64_t kDefaultSize = 64 * 1024;
  static constexpr uint16_t kAudioEncoding = 1;        // PCM
  static constexpr uint16_t kSamplesPerFrame = 2;      // Stereo
  static constexpr uint32_t kFramesPerSecond = 48000;  // 48kHz
  static constexpr uint16_t kBitsPerSample = 16;       // 16-bit samples

  // Writes data to socket_ starting at postion_;
  void WriteToSocket();

  // Writes the header to header_.
  void WriteHeader();

  // Writes a 4CC value into header_.
  void WriteHeader4CC(const std::string& value);

  // Writes a uint16 into header_ in little-endian format.
  void WriteHeaderUint16(uint16_t value);

  // Writes a uint32 into header_ in little-endian format.
  void WriteHeaderUint32(uint32_t value);

  // Gets the positionth byte of the file.
  uint8_t GetByte(size_t position);

  fidl::Binding<SeekingReader> binding_;
  std::vector<uint8_t> header_;
  uint64_t size_ = kDefaultSize;
  zx::socket socket_;
  std::unique_ptr<async::AutoWait> waiter_;
  uint64_t position_;
};

}  // namespace media