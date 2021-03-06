// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/lib/machina/guest.h"

#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <fbl/alloc_checker.h>
#include <fbl/auto_lock.h>
#include <fbl/string_buffer.h>
#include <zircon/device/sysinfo.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/hypervisor.h>
#include <zircon/syscalls/port.h>
#include <zircon/threads.h>

#include "garnet/lib/machina/io.h"
#include "lib/fxl/logging.h"

static constexpr char kResourcePath[] = "/dev/misc/sysinfo";

// Number of threads reading from the async device port.
static constexpr size_t kNumAsyncWorkers = 2;

static zx_status_t guest_get_resource(zx_handle_t* resource) {
  int fd = open(kResourcePath, O_RDWR);
  if (fd < 0) {
    return ZX_ERR_IO;
  }
  ssize_t n = ioctl_sysinfo_get_hypervisor_resource(fd, resource);
  close(fd);
  return n < 0 ? ZX_ERR_IO : ZX_OK;
}

static constexpr uint32_t trap_kind(machina::TrapType type) {
  switch (type) {
    case machina::TrapType::MMIO_SYNC:
      return ZX_GUEST_TRAP_MEM;
    case machina::TrapType::MMIO_BELL:
      return ZX_GUEST_TRAP_BELL;
    case machina::TrapType::PIO_SYNC:
      return ZX_GUEST_TRAP_IO;
    default:
      ZX_PANIC("Unhandled TrapType %d\n", static_cast<int>(type));
      return 0;
  }
}

namespace machina {

zx_status_t Guest::Init(size_t mem_size) {
  zx_status_t status = phys_mem_.Init(mem_size);
  if (status != ZX_OK) {
    FXL_LOG(ERROR) << "Failed to create guest physical memory";
    return status;
  }

  zx_handle_t resource;
  status = guest_get_resource(&resource);
  if (status != ZX_OK) {
    FXL_LOG(ERROR) << "Failed to get hypervisor resource";
    return status;
  }

  status = zx_guest_create(resource, 0, phys_mem_.vmo().get(), &guest_);
  if (status != ZX_OK) {
    FXL_LOG(ERROR) << "Failed to create guest";
    return status;
  }
  zx_handle_close(resource);

  for (size_t i = 0; i < kNumAsyncWorkers; ++i) {
    fbl::StringBuffer<ZX_MAX_NAME_LEN> name_buffer;
    name_buffer.AppendPrintf("io-handler-%zu", i);
    status = device_loop_.StartThread(name_buffer.c_str());
    if (status != ZX_OK) {
      FXL_LOG(ERROR) << "Failed to create async worker";
      return status;
    }
  }

  return ZX_OK;
}

Guest::~Guest() { zx_handle_close(guest_); }

zx_status_t Guest::CreateMapping(TrapType type, uint64_t addr, size_t size,
                                 uint64_t offset, IoHandler* handler) {
  uint32_t kind = trap_kind(type);
  fbl::AllocChecker ac;
  auto mapping = fbl::make_unique_checked<IoMapping>(&ac, kind, addr, size,
                                                     offset, handler);
  if (!ac.check()) {
    return ZX_ERR_NO_MEMORY;
  }

  zx_status_t status = mapping->SetTrap(this);
  if (status != ZX_OK) {
    return status;
  }

  mappings_.push_front(fbl::move(mapping));
  return ZX_OK;
}

void Guest::RegisterVcpuFactory(VcpuFactory factory) {
  vcpu_factory_ = fbl::move(factory);
}

zx_status_t Guest::StartVcpu(uintptr_t entry, uint64_t id) {
  fbl::AutoLock lock(&mutex_);
  if (id >= kMaxVcpus) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (vcpus_[0] == nullptr && id != 0) {
    FXL_LOG(ERROR) << "VCPU-0 must be started before other VCPUs";
    return ZX_ERR_BAD_STATE;
  }
  if (vcpus_[id] != nullptr) {
    // The guest might make multiple requests to start a particular VCPU. On
    // x86, the guest should send two START_UP IPIs but we initialize the VCPU
    // on the first. So, we ignore subsequent requests.
    return ZX_OK;
  }
  auto vcpu = fbl::make_unique<Vcpu>();
  zx_status_t status = vcpu_factory_(this, entry, id, vcpu.get());
  if (status != ZX_OK) {
    return status;
  }
  vcpus_[id] = fbl::move(vcpu);

  return ZX_OK;
}

zx_status_t Guest::SignalInterrupt(uint32_t mask, uint8_t vector) {
  for (size_t id = 0; id != kMaxVcpus; ++id) {
    if (vcpus_[id] == nullptr || !((1u << id) & mask)) {
      continue;
    }
    zx_status_t status = vcpus_[id]->Interrupt(vector);
    if (status != ZX_OK) {
      return status;
    }
  }
  return ZX_OK;
}

zx_status_t Guest::Join() {
  // We assume that the VCPU-0 thread will be started first, and that no
  // additional VCPUs will be brought up after it terminates.
  zx_status_t status = vcpus_[0]->Join();

  // Once the initial VCPU has terminated, wait for any additional VCPUs.
  for (size_t id = 1; id != kMaxVcpus; ++id) {
    if (vcpus_[id] != nullptr) {
      zx_status_t vcpu_status = vcpus_[id]->Join();
      if (vcpu_status != ZX_OK) {
        status = vcpu_status;
      }
    }
  }

  return status;
}

}  // namespace machina
