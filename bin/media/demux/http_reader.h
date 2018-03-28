// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>

#include <fuchsia/cpp/network.h>

#include "garnet/bin/media/demux/reader.h"
#include "garnet/bin/media/fidl/fidl_async_waiter.h"
#include "garnet/bin/media/util/incident.h"
#include "lib/app/cpp/application_context.h"

namespace media {

// Reads from a file on behalf of a demux.
class HttpReader : public Reader {
 public:
  static std::shared_ptr<HttpReader> Create(
      component::ApplicationContext* application_context,
      const std::string& url);

  HttpReader(component::ApplicationContext* application_context,
             const std::string& url);

  ~HttpReader() override;

  // Reader implementation.
  void Describe(DescribeCallback callback) override;

  void ReadAt(size_t position,
              uint8_t* buffer,
              size_t bytes_to_read,
              ReadAtCallback callback) override;

 private:
  // Reads from an open |socket_|.
  void ReadFromSocket();

  // Completes a pending ReadAt.
  void CompleteReadAt(Result result, size_t bytes_read);

  // Fails the pending ReadAt.
  void FailReadAt(zx_status_t status);

  // Fails the pending ReadAt.
  void FailReadAt(Result result);

  // Performs an HTTP load and reads from the resulting socket.
  void LoadAndReadFromSocket();

  std::string url_;
  network::URLLoaderPtr url_loader_;
  Result result_ = Result::kOk;
  uint64_t size_ = kUnknownSize;
  bool can_seek_ = false;
  zx::socket socket_;
  FidlAsyncWaitID wait_id_ = 0;
  size_t socket_position_ = kUnknownSize;
  Incident ready_;

  // Pending ReadAt parameters.
  size_t read_at_position_;
  uint8_t* read_at_buffer_;
  size_t read_at_bytes_to_read_;
  size_t read_at_bytes_remaining_;
  ReadAtCallback read_at_callback_;
};

}  // namespace media