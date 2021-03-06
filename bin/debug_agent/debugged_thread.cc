// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/debug_agent/debugged_thread.h"

#include <inttypes.h>
#include <zircon/syscalls/debug.h>
#include <zircon/syscalls/exception.h>
#include <memory>

#include "garnet/bin/debug_agent/arch.h"
#include "garnet/bin/debug_agent/debug_agent.h"
#include "garnet/bin/debug_agent/debugged_process.h"
#include "garnet/bin/debug_agent/process_breakpoint.h"
#include "garnet/bin/debug_agent/process_info.h"
#include "garnet/bin/debug_agent/unwind.h"
#include "garnet/lib/debug_ipc/agent_protocol.h"
#include "garnet/lib/debug_ipc/helper/stream_buffer.h"
#include "garnet/lib/debug_ipc/message_reader.h"
#include "garnet/lib/debug_ipc/message_writer.h"
#include "garnet/public/lib/fxl/logging.h"

namespace debug_agent {

DebuggedThread::DebuggedThread(DebuggedProcess* process, zx::thread thread,
                               zx_koid_t koid, bool starting)
    : debug_agent_(process->debug_agent()),
      process_(process),
      thread_(std::move(thread)),
      koid_(koid) {
  if (starting) thread_.resume(ZX_RESUME_EXCEPTION);
}

DebuggedThread::~DebuggedThread() {}

void DebuggedThread::OnException(uint32_t type) {
  suspend_reason_ = SuspendReason::kException;

  if (current_breakpoint_) {
    // The current breakpoint is set only when stopped at a breakpoint or when
    // single-stepping over one. We're not going to get an exception for a
    // thread when stopped, so hitting this exception means the breakpoint is
    // done being stepped over. The breakpoint will tell us if the exception
    // was from a normal completion of the breakpoing step, or whether
    // something else went wrong while stepping.
    bool completes_bp_step =
        current_breakpoint_->BreakpointStepHasException(koid_, type);
    current_breakpoint_ = nullptr;
    if (completes_bp_step &&
        after_breakpoint_step_ == AfterBreakpointStep::kContinue) {
      // This step was an internal thing to step over the breakpoint in service
      // of continuing from a breakpoint. Transparently resume the thread since
      // the client didn't request the step.
      Resume(debug_ipc::ResumeRequest::How::kContinue);
      return;
    }
    // Something else went wrong while stepping (the instruction with the
    // breakpoing could have crashed). Fall through to dispatching the
    // exception to the client.
    current_breakpoint_ = nullptr;
  }

  debug_ipc::NotifyException notify;
  zx_thread_state_general_regs regs;
  thread_.read_state(ZX_THREAD_STATE_GENERAL_REGS, &regs, sizeof(regs));

  switch (type) {
    case ZX_EXCP_SW_BREAKPOINT:
      notify.type = debug_ipc::NotifyException::Type::kSoftware;
      UpdateForSoftwareBreakpoint(&regs);
      break;
    case ZX_EXCP_HW_BREAKPOINT:
      notify.type = debug_ipc::NotifyException::Type::kHardware;
      break;
    default:
      notify.type = debug_ipc::NotifyException::Type::kGeneral;
      break;
  }

  notify.process_koid = process_->koid();
  FillThreadRecord(thread_, &notify.thread);
  notify.frame.ip = *arch::IPInRegs(&regs);
  notify.frame.sp = *arch::SPInRegs(&regs);

  // Send notification.
  debug_ipc::MessageWriter writer;
  debug_ipc::WriteNotifyException(notify, &writer);
  debug_agent_->stream()->Write(writer.MessageComplete());

  // Keep the thread suspended for the client.

  // TODO(brettw) suspend other threads in the process and other debugged
  // processes as desired.
}

void DebuggedThread::Pause() {
  if (suspend_reason_ == SuspendReason::kNone) {
    if (thread_.suspend() == ZX_OK) suspend_reason_ = SuspendReason::kOther;
  }
}

void DebuggedThread::Resume(debug_ipc::ResumeRequest::How how) {
  if (suspend_reason_ == SuspendReason::kException) {
    if (current_breakpoint_) {
      // Going over a breakpoint always requires a single-step first. Then we
      // either continue or break.
      SetSingleStep(true);
      if (how == debug_ipc::ResumeRequest::How::kStepInstruction)
        after_breakpoint_step_ = AfterBreakpointStep::kBreak;
      else
        after_breakpoint_step_ = AfterBreakpointStep::kContinue;

      current_breakpoint_->BeginStepOver(koid_);
    } else {
      SetSingleStep(how == debug_ipc::ResumeRequest::How::kStepInstruction);
    }
    suspend_reason_ = SuspendReason::kNone;
    thread_.resume(ZX_RESUME_EXCEPTION);
  } else if (suspend_reason_ == SuspendReason::kOther) {
    // A breakpoint should only be current when it was hit which will be
    // caused by an exception.
    FXL_DCHECK(!current_breakpoint_);

    SetSingleStep(how == debug_ipc::ResumeRequest::How::kStepInstruction);
    suspend_reason_ = SuspendReason::kNone;
    thread_.resume(0);
  }
}

void DebuggedThread::GetBacktrace(
    std::vector<debug_ipc::StackFrame>* frames) const {
  // This call will fail if the thread isn't in a state to get its backtrace.
  zx_thread_state_general_regs regs;
  zx_status_t status =
      thread_.read_state(ZX_THREAD_STATE_GENERAL_REGS, &regs, sizeof(regs));
  if (status != ZX_OK) return;

  constexpr size_t kMaxStackDepth = 256;
  UnwindStack(process_->process(), thread_, *arch::IPInRegs(&regs),
              *arch::SPInRegs(&regs), kMaxStackDepth, frames);
}

void DebuggedThread::SendThreadNotification() const {
  debug_ipc::ThreadRecord record;
  FillThreadRecord(thread_, &record);

  debug_ipc::NotifyThread notify;
  notify.process_koid = process_->koid();
  notify.record = record;

  debug_ipc::MessageWriter writer;
  debug_ipc::WriteNotifyThread(
      debug_ipc::MsgHeader::Type::kNotifyThreadStarting, notify, &writer);
  debug_agent_->stream()->Write(writer.MessageComplete());
}

void DebuggedThread::UpdateForSoftwareBreakpoint(
    zx_thread_state_general_regs* regs) {
  uint64_t breakpoint_address =
      arch::BreakpointInstructionForExceptionAddress(*arch::IPInRegs(regs));

  current_breakpoint_ =
      process_->FindProcessBreakpointForAddr(breakpoint_address);
  if (current_breakpoint_) {
    // When the program hits one of our breakpoints, set the IP back to
    // the exact address that triggered the breakpoint. When the thread
    // resumes, this is the address that it will resume from (after
    // putting back the original instruction), and will be what the client
    // wants to display to the user.
    *arch::IPInRegs(regs) = breakpoint_address;
    zx_status_t status =
        thread_.write_state(ZX_THREAD_STATE_GENERAL_REGS, regs,
                            sizeof(zx_thread_state_general_regs));
    if (status != ZX_OK) {
      fprintf(stderr, "Warning: could not update IP on thread, error = %d.",
              static_cast<int>(status));
    }
  } else {
    // Hit a software breakpoint that doesn't correspond to any current
    // breakpoint.
    if (arch::IsBreakpointInstruction(process_->process(),
                                      breakpoint_address)) {
      // The breakpoint is a hardcoded instruction in the program code. In this
      // case we want to continue from the following instruction since the
      // breakpoint instruction will never go away.
      *arch::IPInRegs(regs) = arch::NextInstructionForSoftwareExceptionAddress(
          *arch::IPInRegs(regs));
      zx_status_t status =
          thread_.write_state(ZX_THREAD_STATE_GENERAL_REGS, regs,
                              sizeof(zx_thread_state_general_regs));
      if (status != ZX_OK) {
        fprintf(stderr, "Warning: could not update IP on thread, error = %d.",
                static_cast<int>(status));
      }
    } else {
      // Not a breakpoint instruction. Probably the breakpoint instruction used
      // to be ours but its removal raced with the exception handler. Resume
      // from the instruction that used to be the breakpoint.
      *arch::IPInRegs(regs) = breakpoint_address;

      // Don't automatically continue execution here. A race for this should be
      // unusual and maybe something weird happened that caused an exception
      // we're not set up to handle. Err on the side of telling the user about
      // the exception.
    }
  }
}

void DebuggedThread::SetSingleStep(bool single_step) {
  zx_thread_state_single_step_t value = single_step ? 1 : 0;
  // This could fail for legitimate reasons, like the process could have just
  // closed the thread.
  thread_.write_state(ZX_THREAD_STATE_SINGLE_STEP, &value, sizeof(value));
}

}  // namespace debug_agent
