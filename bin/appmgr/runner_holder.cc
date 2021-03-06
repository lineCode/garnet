// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/appmgr/runner_holder.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "garnet/bin/appmgr/component_controller_impl.h"
#include "garnet/bin/appmgr/realm.h"
#include "garnet/bin/appmgr/util.h"
#include "lib/fsl/vmo/file.h"

namespace fuchsia {
namespace sys {

RunnerHolder::RunnerHolder(Services services, ComponentControllerPtr controller,
                           LaunchInfo launch_info, Realm* realm,
                           std::function<void()> error_handler)
    : services_(std::move(services)),
      controller_(std::move(controller)),
      impl_object_(nullptr),
      error_handler_(error_handler),
      component_id_counter_(0) {
  realm->CreateComponent(std::move(launch_info), controller_.NewRequest(),
                         [this](ComponentControllerImpl* component) {
                           CreateComponentCallback(component);
                         });

  controller_.set_error_handler([this] {
    Cleanup();

    if (error_handler_) {
      error_handler_();
    }
  });

  services_.ConnectToService(runner_.NewRequest());
}

RunnerHolder::~RunnerHolder() = default;

void RunnerHolder::Cleanup() {
  impl_object_ = nullptr;
  components_.clear();
}

void RunnerHolder::CreateComponentCallback(ComponentControllerImpl* component) {
  impl_object_ = component;
  koid_ = component->koid();

  // add error handler
  impl_object_->Wait([this](int exit_code) { Cleanup(); });

  // update hub
  for (auto& n : components_) {
    n.second->SetParentJobId(koid_);
    impl_object_->AddSubComponentHub(n.second->HubInfo());
  }
}

void RunnerHolder::StartComponent(
    Package package, StartupInfo startup_info, fxl::RefPtr<Namespace> ns,
    fidl::InterfaceRequest<ComponentController> controller) {
  auto url = startup_info.launch_info.url;
  const std::string args =
      util::GetArgsString(startup_info.launch_info.arguments);
  auto channels = util::BindDirectory(&startup_info.launch_info);

  ComponentControllerPtr remote_controller;
  auto remote_controller_request = remote_controller.NewRequest();

  // TODO(anmittal): Create better unique instance id, instead of 1,2,3,4,...
  auto component = std::make_unique<ComponentBridge>(
      std::move(controller), std::move(remote_controller), this, url,
      std::move(args), util::GetLabelFromURL(url),
      std::to_string(++component_id_counter_), std::move(ns),
      ExportedDirType::kLegacyFlatLayout, std::move(channels.exported_dir),
      std::move(channels.client_request));

  // update hub
  if (impl_object_) {
    component->SetParentJobId(koid_);
    impl_object_->AddSubComponentHub(component->HubInfo());
  }

  ComponentBridge* key = component.get();
  components_.emplace(key, std::move(component));

  runner_->StartComponent(std::move(package), std::move(startup_info),
                          std::move(remote_controller_request));
}

std::unique_ptr<ComponentBridge> RunnerHolder::ExtractComponent(
    ComponentBridge* controller) {
  auto it = components_.find(controller);
  if (it == components_.end()) {
    return nullptr;
  }
  auto component = std::move(it->second);

  // update hub
  if (impl_object_) {
    impl_object_->RemoveSubComponentHub(component->HubInfo());
  }

  components_.erase(it);
  return component;
}

}  // namespace sys
}  // namespace fuchsia
