// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/ui/root_presenter/presentation_switcher.h"

#include "garnet/bin/ui/root_presenter/presentation.h"

namespace root_presenter {

bool PresentationSwitcher::OnEvent(const input::InputEvent& event,
                                   Presentation* presentation) {
  const input::KeyboardEvent& kbd = event.keyboard();
  if (kbd.modifiers & input::kModifierControl &&
      kbd.modifiers & input::kModifierAlt &&
      kbd.phase == input::KeyboardEventPhase::PRESSED) {
    if (kbd.code_point == 91 /* [ */) {
      presentation->yield_callback_(/* yield_to_next= */ false);
      return true;
    } else if (kbd.code_point == 93 /* ] */) {
      presentation->yield_callback_(/* yield_to_next= */ true);
      return true;
    }
  }
  return false;
}

}  // namespace root_presenter