// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_BIN_HTTP_NETWORK_SERVICE_IMPL_H_
#define GARNET_BIN_HTTP_NETWORK_SERVICE_IMPL_H_

#include <list>
#include <queue>

#include <network/cpp/fidl.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/zx/channel.h>

#include "garnet/bin/http/url_loader_impl.h"
#include "lib/fidl/cpp/binding_set.h"
#include "lib/fidl/cpp/interface_request.h"

namespace network {

class NetworkServiceImpl : public NetworkService,
                           public URLLoaderImpl::Coordinator {
 public:
  NetworkServiceImpl(async_t* dispatcher);
  ~NetworkServiceImpl() override;

  void AddBinding(fidl::InterfaceRequest<NetworkService> request);

  // NetworkService methods:
  void CreateURLLoader(fidl::InterfaceRequest<URLLoader> request) override;
  void GetCookieStore(zx::channel cookie_store) override;
  void CreateWebSocket(zx::channel socket) override;

 private:
  class UrlLoaderContainer;

  // URLLoaderImpl::Coordinator:
  void RequestNetworkSlot(
      fit::function<void(fit::closure)> slot_request) override;

  void OnSlotReturned();

  async_t* const dispatcher_;
  size_t available_slots_;
  fidl::BindingSet<NetworkService> bindings_;
  std::list<UrlLoaderContainer> loaders_;
  std::queue<fit::function<void(fit::closure)>> slot_requests_;
};

}  // namespace network

#endif  // GARNET_BIN_HTTP_NETWORK_SERVICE_IMPL_H_