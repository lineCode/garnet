// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>

#include <fbl/auto_lock.h>
#include <fbl/mutex.h>
#include <fbl/vector.h>
#include <garnet/examples/ui/video_display/camera_interface_base.h>
#include <lib/async/cpp/wait.h>
#include <lib/fxl/logging.h>
#include <zircon/device/camera-proto.h>
#include <zircon/device/camera.h>
#include <zircon/types.h>
#include <zx/channel.h>
#include <zx/vmo.h>

namespace video_display {

// The method for setting up a camera stream is as follows:
// 1) client.Open(device_id)
// 2) client.GetSupportedFormats
// 3) --> Receive GetSupportedFormats callback with formats
// 4) client.SetFormat(The format you select)
// 5) --> Receive SetFormat callback with maximum frame size
// 6) SetBuffer(vmo you allocate)
// 6.5) --> Receive SetBuffer callback (optional, since CameraClient will
//          serialize the SetBuffer and start commands
// 7) Start
// ----------------------
// Normal operation:
//  ---> Receive FrameNotifyCallback when a frame is available
//  Consume the frame
//  client.ReleaseFrame

// CameraClient is not generally thread safe.
// CameraClient does have some thread safe properties, assuming that:
//  1) There is only one thread handling the channel messages
//  2) There is only one thread calling the public interface functions
class CameraClient : public CameraInterfaceBase {
 public:
  CameraClient();
  virtual ~CameraClient();

  // Get the supported formats for the camera.
  // get_formats_callback will be called in response.
  zx_status_t GetSupportedFormats(GetFormatCallback get_formats_callback);

  // Set the format of the video stream.  This function
  // should only be called once in the setup of the stream.
  zx_status_t SetFormat(const camera_video_format_t& format,
                        SetFormatCallback set_format_callback);

  // Set the vmo that will be used to pass video information from the driver
  // to the consumer.  The entire vmo is assumed to be available.  The driver
  // will provide the offset into the vmo at which a frame has been written.
  // This function should only be called once in the setup of the stream.
  zx_status_t SetBuffer(const zx::vmo& vmo);

  // Signal the driver to start producing frames.  Upon each frame being
  // written, |frame_notify_callback| will be called. Start can be called
  // immediatly after calling SetBuffer.
  // This function should only be called once in the setup of the stream.
  zx_status_t Start(FrameNotifyCallback frame_notify_callback);

  // Release the lock on a video frame.  The data_offset corresponds to the
  // data_vb_offset field given in the camera_vb_frame_notify_t passed to
  // the frame_notify_callback.
  zx_status_t ReleaseFrame(uint64_t data_offset);

  // Signal the driver that it should stop producing frames.
  // Frame notifications may occur after calling this function.
  zx_status_t Stop();

  // Open a device in /dev/class/camera/ with the ID of dev_id.
  zx_status_t Open(uint32_t dev_id);

  // Close the stream in a hackish sort of way.
  // TODO(garratt): Add OnShutdown() callback and ShutdownChannels()
  // to handle the asyncronous dance of shutting down properly.
  // See the comment for Close() in camera_client.cc.
  void Close();

  // Called by the client when the consumer has released all the handles.
  void ShutDown() {
    bool waiting_for_driver = true, waiting_for_consumer;
    // See if we are shutting down already. If not, we'll call close
    if (!IsShuttingDown(&waiting_for_driver, &waiting_for_consumer)) {
      Close();
    }
    // either way, we are not waiting for the consumer anymore:
    SetShuttingDown(waiting_for_driver, false);
  }

 private:
  // The callbacks functions which get called upon incoming messages over the
  // channel:
  zx_status_t OnGetFormatsResp(camera::camera_proto::GetFormatsResp resp);
  zx_status_t OnSetFormatResp(camera::camera_proto::SetFormatResp resp,
                              zx::channel ch);
  zx_status_t OnFrameNotify(camera::camera_proto::VideoBufFrameNotify resp);

  // Called when a new message is received on the Command channel:
  void OnNewCmdMessage(async_t* async,
                       async::WaitBase* wait,
                       zx_status_t status,
                       const zx_packet_signal* signal);
  zx_status_t ProcessCmdChannel();

  // Called when a new message is received on the Streaming channel:
  void OnNewBufferMessage(async_t* async,
                          async::WaitBase* wait,
                          zx_status_t status,
                          const zx_packet_signal* signal);
  zx_status_t ProcessBufferChannel();

  // Send the stop command to the driver.  This differs from the public Stop()
  // which deals with the state of the driver.
  zx_status_t SendStop();

  enum CameraState : uint16_t {
      Closed             = 0x0000,
      Error              = 0x0100,
      Configuring        = 0x0200,
      // All the states we step through during configuration:
      CommandChannelOpen = 0x0201,
      FormatsRequested   = 0x0203,
      FormatsReceived    = 0x0205,
      SetFormatRequested = 0x0207,
      SetFormatReceived  = 0x0211,
      BufferChannelOpen  = 0x0210,
      SetBufferRequested = 0x0231,
      StartRequested     = 0x0251,
      ChannelsMask       = 0x0011,

      Streaming          = 0x0400,
      ShuttingDown       = 0x0800,
      // The Shutdown states:
      WaitingForDriver   = 0x0801,
      WaitingForConsumer = 0x0802,
      WaitingForBoth     = 0x0803
  };

  // ---------  State Machine Functions  ---------------
  // All the state machine functions are guarded by the state_lock_ mutex.
  // This allows us to ensure atomic state transitions.
  // Verify that we are in a given state:
  zx_status_t CheckConfigurationState(CameraState required_state);
  // Set the next state.  This first checks that we are in the current_state,
  // so we can detect (if called in a multithreaded fashion) if a state had
  // changed during the call.
  void SetConfigurationState(CameraState current_state, CameraState next_state);

  // Check the various main states:
  bool IsStreaming();
  bool IsConfiguring();
  bool IsClosed();

  void SetStreaming();

  // See the description of the Shutdown procedure. When shutting down, we must
  // wait for the driver and the consumer to release resources.  These
  // functions break out the two waiting states for easy manipulation.
  void SetShuttingDown(bool waiting_for_driver = false,
                       bool waiting_for_consumer = false);
  // If we are not in a shutting down state, IsShuttingDown will return false,
  // and the two variables will not be touched.
  bool IsShuttingDown(bool* waiting_for_driver = nullptr,
                      bool* waiting_for_consumer = nullptr);

  // Callbacks for asyncronous operations.
  // These functions are also used to determine state; if they are defined,
  // then we are waiting for the appropriate response
  SetFormatCallback set_format_callback_ = nullptr;
  GetFormatCallback get_formats_callback_ = nullptr;
  FrameNotifyCallback frame_notify_callback_ = nullptr;

  CameraState state_ __TA_GUARDED(state_lock_) = CameraState::Closed;
  fbl::Mutex state_lock_;

  zx::channel stream_ch_;
  zx::channel vb_ch_;

  std::vector<camera_video_format_t> out_formats_;
  async::WaitMethod<CameraClient, &CameraClient::OnNewCmdMessage>
      cmd_msg_waiter_{this};
  async::WaitMethod<CameraClient, &CameraClient::OnNewBufferMessage>
      buff_msg_waiter_{this};
};

}  // namespace video_display
