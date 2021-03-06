// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/zxdb/console/verbs.h"

#include <iomanip>
#include <sstream>

#include "garnet/bin/zxdb/client/arch_info.h"
#include "garnet/bin/zxdb/client/disassembler.h"
#include "garnet/bin/zxdb/client/err.h"
#include "garnet/bin/zxdb/client/frame.h"
#include "garnet/bin/zxdb/client/memory_dump.h"
#include "garnet/bin/zxdb/client/process.h"
#include "garnet/bin/zxdb/client/session.h"
#include "garnet/bin/zxdb/client/symbols/location.h"
#include "garnet/bin/zxdb/client/system.h"
#include "garnet/bin/zxdb/client/target.h"
#include "garnet/bin/zxdb/console/command.h"
#include "garnet/bin/zxdb/console/command_utils.h"
#include "garnet/bin/zxdb/console/console.h"
#include "garnet/bin/zxdb/console/memory_format.h"
#include "garnet/bin/zxdb/console/output_buffer.h"

namespace zxdb {

namespace {

constexpr int kSizeSwitch = 1;
constexpr int kNumSwitch = 2;
constexpr int kRawSwitch = 3;

// mem-read --------------------------------------------------------------------

void MemoryReadComplete(const Err& err, MemoryDump dump) {
  OutputBuffer out;
  if (err.has_error()) {
    out.OutputErr(err);
  } else {
    MemoryFormatOptions opts;
    opts.show_addrs = true;
    opts.show_ascii = true;
    opts.values_per_line = 16;
    opts.separator_every = 8;
    out.Append(FormatMemory(dump, dump.address(),
                            static_cast<uint32_t>(dump.size()), opts));
  }
  Console::get()->Output(std::move(out));
}

const char kMemReadShortHelp[] =
    R"(mem-read / x: Read memory from debugged process.)";
const char kMemReadHelp[] =
    R"(mem-read [ --size=<bytes> ] <address>

  Alias: "x"

  Reads memory from the process at the given address and prints it to the
  screen. Currently, only a byte-oriented hex dump format is supported.

Arguments

  --size=<bytes> | -s <bytes>
    Bytes to read. This defaults to 64 if unspecified.

Examples

  x --size=128 0x75f19ba
  mem-read --size=16 0x8f1763a7
  process 3 mem-read 83242384560
)";
Err DoMemRead(ConsoleContext* context, const Command& cmd) {
  // Only a process can have its memory read.
  Err err = cmd.ValidateNouns({Noun::kProcess});
  if (err.has_error())
    return err;

  err = AssertRunningTarget(context, "mem-read", cmd.target());
  if (err.has_error())
    return err;

  // Address (required).
  uint64_t address = 0;
  if (cmd.args().size() != 1) {
    return Err(ErrType::kInput,
               "mem-read requires exactly one argument that "
               "is the address to read.");
  }
  err = StringToUint64(cmd.args()[0], &address);
  if (err.has_error())
    return err;

  // Size argument (optional).
  uint64_t size = 64;
  if (cmd.HasSwitch(kSizeSwitch)) {
    err = StringToUint64(cmd.GetSwitchValue(kSizeSwitch), &size);
    if (err.has_error())
      return err;
  }

  cmd.target()->GetProcess()->ReadMemory(address, size, &MemoryReadComplete);
  return Err();
}

// disassemble -----------------------------------------------------------------

// Completion callback after reading process memory.
void CompleteDisassemble(const Err& err, MemoryDump dump, uint64_t num_instr,
                         fxl::WeakPtr<Process> weak_process,
                         Disassembler::Options options) {
  Console* console = Console::get();
  if (err.has_error()) {
    console->Output(err);
    return;
  }

  if (!weak_process)
    return;  // Give up if the process went away.

  // Make the disassembler.
  Disassembler disassembler;
  Session* session = console->context().session();
  Err my_err = disassembler.Init(session->arch_info());
  if (my_err.has_error()) {
    console->Output(err);
    return;
  }

  std::vector<std::vector<std::string>> rows;
  disassembler.DisassembleDump(dump, options, num_instr, &rows);

  std::vector<ColSpec> spec;
  if (options.emit_addresses) {
    spec.emplace_back(Align::kRight);
    spec.back().syntax = Syntax::kComment;
  }
  if (options.emit_bytes) {
    // Max out the bytes @ 17 cols (holds 6 bytes) to keep it from pushing
    // things too far over in the common case.
    spec.emplace_back(Align::kLeft, 17, std::string(), 1);
    spec.back().syntax = Syntax::kComment;
  }

  spec.emplace_back(Align::kLeft, 0, std::string(), 1);  // Instructions.

  // Params. Some can be very long so provide a max so the comments don't get
  // pushed too far out.
  spec.emplace_back(Align::kLeft, 10, std::string(), 1);
  spec.emplace_back(Align::kLeft);  // Comments.
  spec.back().syntax = Syntax::kComment;

  // Left column (whatever it is) gets 2 spaces padding for indentation.
  spec[0].pad_left = 2;

  OutputBuffer out;
  FormatColumns(spec, rows, &out);
  console->Output(std::move(out));
}

const char kDisassembleShortHelp[] =
    "disassemble / di: Disassemble machine instructions.";
const char kDisassembleHelp[] =
    R"(disassemble [ --num=<lines> ] [ --raw ] [ <start_address> ]

  Alias: "di"

  Disassembles machine instructions at the given address. If no address is
  given, the instruction pointer of the thread/frame will be used. If the
  thread is not stopped, you must specify a start address.

Arguments

  --num=<lines> | -n <lines>
      The number of lines/instructions to emit. Defaults to 16.

  --raw | -r
      Output raw bytes in addition to the decoded instructions.

Examples

  di
  disassemble
      Disassembles starting at the current thread's instruction pointer.

  thread 3 disassemble -n 128
      Disassembles 128 instructions starting at thread 3's instruction
      pointer.

  frame 3 disassemble
  thread 2 frame 3 disassemble
      Disassembles starting at the thread's "frame 3" instruction pointer
      (which will be the call return address).

  process 1 disassemble 0x7b851239a0
      Disassembles instructions in process 1 starting at the given address.
)";
Err DoDisassemble(ConsoleContext* context, const Command& cmd) {
  // Can take process overrides (to specify which process to read) and thread
  // and frame ones (to specify which thread to read the instruction pointer
  // from).
  Err err = cmd.ValidateNouns({Noun::kProcess, Noun::kThread, Noun::kFrame});
  if (err.has_error())
    return err;

  err = AssertRunningTarget(context, "disassemble", cmd.target());
  if (err.has_error())
    return err;

  uint64_t address = 0;
  if (cmd.args().empty()) {
    // No args: implicitly read the frame's instruction pointer.
    Frame* frame = cmd.frame();
    if (!frame) {
      return Err(
          "There is no frame to read the instruction pointer from. The thread\n"
          "must be stopped to use the implicit current address. Otherwise,\n"
          "you must supply an explicit address to disassemble.");
    }
    address = frame->GetLocation().address();
  } else if (cmd.args().size() == 1) {
    // One argument is the address to read.
    err = StringToUint64(cmd.args()[0], &address);
    if (err.has_error())
      return err;
  } else {
    // More arguments are errors.
    return Err(ErrType::kInput, "\"disassemble\" takes at most one argument.");
  }

  Disassembler::Options options;
  options.emit_addresses = true;
  options.emit_bytes = cmd.HasSwitch(kRawSwitch);
  options.emit_undecodable = true;

  // Num argument (optional).
  uint64_t num_instr = 16;
  if (cmd.HasSwitch(kNumSwitch)) {
    err = StringToUint64(cmd.GetSwitchValue(kNumSwitch), &num_instr);
    if (err.has_error())
      return err;
  }

  // Compute the max bytes requires to get the requested instructions.
  // It doesn't matter if we request more memory than necessary so use a
  // high bound.
  size_t size = num_instr * context->session()->arch_info()->max_instr_len();

  // Schedule memory request.
  Process* process = cmd.target()->GetProcess();
  process->ReadMemory(
      address, size, [ num_instr, options, process = process->GetWeakPtr() ](
                         const Err& err, MemoryDump dump) {
        CompleteDisassemble(err, std::move(dump), num_instr, process, options);
      });
  return Err();
}

}  // namespace

void AppendMemoryVerbs(std::map<Verb, VerbRecord>* verbs) {
  // "x" is the GDB command to read memory.
  VerbRecord mem_read(&DoMemRead, {"mem-read", "x"}, kMemReadShortHelp,
                      kMemReadHelp);
  mem_read.switches.push_back(SwitchRecord(kSizeSwitch, true, "size", 's'));
  (*verbs)[Verb::kMemRead] = std::move(mem_read);

  VerbRecord disass(&DoDisassemble, {"disassemble", "di"},
                    kDisassembleShortHelp, kDisassembleHelp);
  disass.switches.push_back(SwitchRecord(kNumSwitch, true, "num", 'n'));
  disass.switches.push_back(SwitchRecord(kRawSwitch, false, "raw", 'r'));
  (*verbs)[Verb::kDisassemble] = std::move(disass);
}

}  // namespace zxdb
