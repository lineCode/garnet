// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/async/default.h>
#include <lib/fdio/util.h>

#include "echo_client_app.h"
#include "lib/app/cpp/testing/fake_service.h"
#include "lib/app/cpp/testing/test_with_context.h"

namespace echo2 {
namespace testing {

using namespace fidl::examples::echo;

class FakeEcho : public Echo, public fuchsia::sys::testing::FakeService<Echo> {
 public:
  static const std::string URL_;

  FakeEcho() : FakeService(this){};

  void EchoString(fidl::StringPtr value, EchoStringCallback callback) {
    callback(answer_);
  }

  void SetAnswer(fidl::StringPtr answer) { answer_ = answer; }

  void Register(fuchsia::sys::testing::FakeLauncher& fake_launcher) {
    FakeService<Echo>::Register(URL_, fake_launcher);
  }

 private:
  fidl::StringPtr answer_;
};

const std::string FakeEcho::URL_ = "fake-echo";

class EchoClientAppForTest : public EchoClientApp {
 public:
  EchoClientAppForTest(std::unique_ptr<fuchsia::sys::StartupContext> context)
      : EchoClientApp(std::move(context)) {}
};

class EchoClientAppTest : public fuchsia::sys::testing::TestWithContext {
 public:
  void SetUp() override {
    TestWithContext::SetUp();
    echoClientApp_.reset(new EchoClientAppForTest(TakeContext()));
    fake_echo_.reset(new FakeEcho());
    fake_echo_->Register(controller().fake_launcher());
  }

  void TearDown() override {
    echoClientApp_.reset();
    TestWithContext::TearDown();
  }

 protected:
  void Start(std::string server_url) { echoClientApp_->Start(server_url); }
  EchoPtr& echo() { return echoClientApp_->echo(); }
  void SetAnswer(fidl::StringPtr answer) { fake_echo_->SetAnswer(answer); }

 private:
  std::unique_ptr<EchoClientAppForTest> echoClientApp_;
  std::unique_ptr<FakeEcho> fake_echo_;
};  // namespace testing

// Answer "Hello World" with "Goodbye World"
TEST_F(EchoClientAppTest, EchoString_HelloWorld_GoodbyeWorld) {
  fidl::StringPtr message = "bogus";
  Start(FakeEcho::URL_);
  SetAnswer("Goodbye World!");
  echo()->EchoString("Hello World!",
                     [&](::fidl::StringPtr retval) { message = retval; });
  RunLoopUntilIdle();
  EXPECT_EQ("Goodbye World!", message);
}

// Talking to remote without starting it first doesn't work
TEST_F(EchoClientAppTest, EchoString_NoStart) {
  fidl::StringPtr message = "bogus";
  echo()->EchoString("Hello World!",
                     [&](::fidl::StringPtr retval) { message = retval; });
  RunLoopUntilIdle();
  EXPECT_EQ("bogus", message);
}

}  // namespace testing
}  // namespace echo2
