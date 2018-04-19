// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "lib/fxl/synchronization/waitable_event.h"

#include "garnet/lib/ui/gfx/gfx_system.h"
#include "garnet/lib/ui/gfx/tests/gfx_test.h"
#include "garnet/lib/ui/gfx/tests/util.h"
#include "lib/escher/flib/release_fence_signaller.h"
#include "lib/ui/scenic/fidl_helpers.h"
#include "lib/ui/tests/test_with_message_loop.h"

namespace scenic {
namespace gfx {
namespace test {

TEST_F(GfxSystemTest, DISABLED_CreateAndDestroySession) {
  EXPECT_EQ(0U, scenic()->num_sessions());

  ui::SessionPtr session;

  EXPECT_EQ(0U, scenic()->num_sessions());

  scenic()->CreateSession(session.NewRequest(), nullptr);

  RUN_MESSAGE_LOOP_UNTIL(scenic()->num_sessions() == 1);
  session = nullptr;
  RUN_MESSAGE_LOOP_UNTIL(scenic()->num_sessions() == 0);
}

TEST_F(GfxSystemTest, DISABLED_ScheduleUpdateInOrder) {
  // Create a session.
  ui::SessionPtr session;
  EXPECT_EQ(0U, scenic()->num_sessions());
  scenic()->CreateSession(session.NewRequest(), nullptr);
  RUN_MESSAGE_LOOP_UNTIL(scenic()->num_sessions() == 1);
  // Present on the session with presentation_time = 1.
  ui::Session::PresentCallback callback = [](auto) {};
  session->Present(1, CreateEventArray(1), CreateEventArray(1), callback);
  // Briefly pump the message loop. Expect that the session is not destroyed.
  ::scenic::test::RunLoopWithTimeout(kPumpMessageLoopDuration);
  RUN_MESSAGE_LOOP_UNTIL(scenic()->num_sessions() == 1);
  // Present with the same presentation time.
  session->Present(1, CreateEventArray(1), CreateEventArray(1), callback);
  // Briefly pump the message loop. Expect that the session is not destroyed.
  ::scenic::test::RunLoopWithTimeout(kPumpMessageLoopDuration);
  RUN_MESSAGE_LOOP_UNTIL(scenic()->num_sessions() == 1);
}

bool IsFenceSignalled(const zx::event& fence) {
  zx_signals_t signals = 0u;
  zx_status_t status =
      fence.wait_one(escher::kFenceSignalled, zx::time(), &signals);
  FXL_DCHECK(status == ZX_OK || status == ZX_ERR_TIMED_OUT);
  return signals & escher::kFenceSignalled;
}

TEST_F(GfxSystemTest, DISABLED_ReleaseFences) {
  // Tests creating a session, and calling Present with two release fences.
  // The release fences should be signalled after a subsequent Present.
  ui::SessionPtr session;
  EXPECT_EQ(0U, scenic()->num_sessions());
  scenic()->CreateSession(session.NewRequest(), nullptr);
  RUN_MESSAGE_LOOP_UNTIL(scenic()->num_sessions() == 1);
  auto handler = static_cast<SessionHandlerForTest*>(
      gfx_system()->engine()->session_manager()->FindSession(1));
  {
    ::fidl::VectorPtr<ui::Command> commands;
    commands.push_back(
        scenic_lib::NewCommand(scenic_lib::NewCreateCircleCommand(1, 50.f)));
    commands.push_back(
        scenic_lib::NewCommand(scenic_lib::NewCreateCircleCommand(2, 25.f)));

    session->Enqueue(std::move(commands));
  }
  RUN_MESSAGE_LOOP_UNTIL(handler->enqueue_count() == 1);
  EXPECT_EQ(1u, handler->enqueue_count());
  // Create release fences
  ::fidl::VectorPtr<zx::event> release_fences = CreateEventArray(2);
  zx::event release_fence1 = CopyEvent(release_fences->at(0));
  zx::event release_fence2 = CopyEvent(release_fences->at(1));
  EXPECT_FALSE(IsFenceSignalled(release_fence1));
  EXPECT_FALSE(IsFenceSignalled(release_fence2));
  // Call Present with release fences.
  session->Present(0u, ::fidl::VectorPtr<zx::event>::New(0),
                   std::move(release_fences),
                   [](images::PresentationInfo info) {});
  RUN_MESSAGE_LOOP_UNTIL(handler->present_count() == 1);
  EXPECT_EQ(1u, handler->present_count());
  EXPECT_FALSE(IsFenceSignalled(release_fence1));
  EXPECT_FALSE(IsFenceSignalled(release_fence2));
  // Call Present again with no release fences.
  session->Present(0u, ::fidl::VectorPtr<zx::event>::New(0),
                   ::fidl::VectorPtr<zx::event>::New(0),
                   [](images::PresentationInfo info) {});
  RUN_MESSAGE_LOOP_UNTIL(handler->present_count() == 2);
  EXPECT_EQ(2u, handler->present_count());
  RUN_MESSAGE_LOOP_UNTIL(IsFenceSignalled(release_fence1));
  EXPECT_TRUE(IsFenceSignalled(release_fence2));
}

TEST_F(GfxSystemTest, DISABLED_AcquireAndReleaseFences) {
  // Tests creating a session, and calling Present with an acquire and a release
  // fence. The release fences should be signalled only after a subsequent
  // Present, and not until the acquire fence has been signalled.
  ui::SessionPtr session;
  EXPECT_EQ(0U, scenic()->num_sessions());
  scenic()->CreateSession(session.NewRequest(), nullptr);
  RUN_MESSAGE_LOOP_UNTIL(scenic()->num_sessions() == 1);
  auto handler = static_cast<SessionHandlerForTest*>(
      gfx_system()->engine()->session_manager()->FindSession(1));
  {
    ::fidl::VectorPtr<ui::Command> commands;
    commands.push_back(
        scenic_lib::NewCommand(scenic_lib::NewCreateCircleCommand(1, 50.f)));
    commands.push_back(
        scenic_lib::NewCommand(scenic_lib::NewCreateCircleCommand(2, 25.f)));

    session->Enqueue(std::move(commands));
  }
  RUN_MESSAGE_LOOP_UNTIL(handler->enqueue_count() == 1);
  EXPECT_EQ(1u, handler->enqueue_count());
  // Create acquire and release fences
  zx::event acquire_fence;
  ASSERT_EQ(ZX_OK, zx::event::create(0, &acquire_fence));
  zx::event release_fence;
  ASSERT_EQ(ZX_OK, zx::event::create(0, &release_fence));
  ::fidl::VectorPtr<zx::event> acquire_fences;
  acquire_fences.push_back(CopyEvent(acquire_fence));
  ::fidl::VectorPtr<zx::event> release_fences;
  release_fences.push_back(CopyEvent(release_fence));
  // Call Present with both the acquire and release fences.
  session->Present(0u, std::move(acquire_fences), std::move(release_fences),
                   [](images::PresentationInfo info) {});
  RUN_MESSAGE_LOOP_UNTIL(handler->present_count() == 1);
  EXPECT_EQ(1u, handler->present_count());
  EXPECT_FALSE(IsFenceSignalled(release_fence));
  // Call Present again with no fences.
  session->Present(0u, ::fidl::VectorPtr<zx::event>::New(0),
                   ::fidl::VectorPtr<zx::event>::New(0),
                   [](images::PresentationInfo info) {});
  RUN_MESSAGE_LOOP_UNTIL(handler->present_count() == 2);
  EXPECT_FALSE(IsFenceSignalled(release_fence));
  // Now signal the acquire fence.
  acquire_fence.signal(0u, escher::kFenceSignalled);
  // Now expect that the first frame was presented, and its release fence was
  // signalled.
  RUN_MESSAGE_LOOP_UNTIL(IsFenceSignalled(release_fence));
}

}  // namespace test
}  // namespace gfx
}  // namespace scenic