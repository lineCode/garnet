// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/media/audio_server/audio_output.h"

#include "garnet/bin/media/audio_server/audio_device_manager.h"
#include "garnet/bin/media/audio_server/audio_link.h"
#include "lib/fxl/logging.h"
#include "lib/fxl/time/time_delta.h"

namespace media {
namespace audio {

AudioOutput::AudioOutput(AudioDeviceManager* manager)
    : AudioDevice(Type::Output, manager), db_gain_(0.0f) {}

}  // namespace audio
}  // namespace media
