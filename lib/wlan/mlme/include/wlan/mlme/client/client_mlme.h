// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <wlan/mlme/mlme.h>

#include <fbl/ref_ptr.h>

#include <fuchsia/wlan/stats/cpp/fidl.h>
#include <wlan/protocol/mac.h>
#include <zircon/types.h>

namespace wlan {

class DeviceInterface;
class Packet;
class Scanner;
class Station;

// ClientMlme is a MLME which operates in non-AP role. It is not thread-safe.
class ClientMlme : public Mlme {
   public:
    explicit ClientMlme(DeviceInterface* device);
    ~ClientMlme();

    // Mlme interface methods.
    zx_status_t Init() override;
    zx_status_t PreChannelChange(wlan_channel_t chan) override;
    zx_status_t PostChannelChange() override;
    zx_status_t HandleTimeout(const ObjectId id) override;
    // MLME-JOIN.request will initialize a Station and starts the association flow.
    zx_status_t HandleMlmeJoinReq(const ::fuchsia::wlan::mlme::JoinRequest& msg) override;
    ::fuchsia::wlan::stats::ClientMlmeStats GetClientMlmeStats() const override final;

    bool IsStaValid() const;

    DeviceInterface* const device_;

    fbl::RefPtr<Scanner> scanner_;
    // TODO(tkilbourn): track other STAs
    fbl::RefPtr<Station> sta_;
};

}  // namespace wlan
