// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helper/command_buffer_helper.h"
#include "helper/platform_device_helper.h"
#include "magma_arm_mali_types.h"
#include "sys_driver/magma_driver.h"
#include "sys_driver/magma_system_connection.h"
#include "sys_driver/magma_system_context.h"
#include "gtest/gtest.h"

namespace {

class Test {
public:
    void TestValidImmediate()
    {
        auto ctx = InitializeContext();
        ASSERT_TRUE(ctx);

        magma_arm_mali_atom atom[2];
        atom[0].atom_number = 1;
        atom[0].flags = 1;
        atom[0].dependencies[0].atom_number = 0;
        atom[0].dependencies[1].atom_number = 0;
        atom[1].atom_number = 2;
        atom[1].flags = 1;
        atom[1].dependencies[0].atom_number = 1;
        atom[1].dependencies[0].type = kArmMaliDependencyOrder;
        atom[1].dependencies[1].atom_number = 0;

        magma::Status status = ctx->ExecuteImmediateCommands(sizeof(atom), &atom, 0, nullptr);
        EXPECT_EQ(MAGMA_STATUS_OK, status.get());
    }

    void TestInvalidInUse()
    {
        auto ctx = InitializeContext();
        ASSERT_TRUE(ctx);

        magma_arm_mali_atom atom[2];
        atom[0].atom_number = 0;
        atom[0].flags = 1;
        atom[0].dependencies[0].atom_number = 0;
        atom[0].dependencies[1].atom_number = 0;
        atom[1].atom_number = 0;
        atom[1].flags = 1;
        atom[1].dependencies[0].atom_number = 0;
        atom[1].dependencies[1].atom_number = 0;

        magma::Status status = ctx->ExecuteImmediateCommands(sizeof(atom), &atom, 0, nullptr);
        // There's no device thread, so the atoms shouldn't be able to complete.
        EXPECT_EQ(MAGMA_STATUS_CONTEXT_KILLED, status.get());
    }

    void TestInvalidDependencyNotSubmitted()
    {
        auto ctx = InitializeContext();
        ASSERT_TRUE(ctx);

        magma_arm_mali_atom atom;
        atom.atom_number = 1;
        atom.flags = 1;
        // Can't depend on self or on later atoms.
        atom.dependencies[0].atom_number = 1;
        atom.dependencies[0].type = kArmMaliDependencyOrder;
        atom.dependencies[1].atom_number = 0;

        magma::Status status = ctx->ExecuteImmediateCommands(sizeof(atom), &atom, 0, nullptr);
        EXPECT_EQ(MAGMA_STATUS_CONTEXT_KILLED, status.get());
    }

    void TestInvalidDependencyType()
    {
        auto ctx = InitializeContext();
        ASSERT_TRUE(ctx);

        magma_arm_mali_atom atom[2];
        atom[0].atom_number = 1;
        atom[0].flags = 1;
        atom[0].dependencies[0].atom_number = 0;
        atom[0].dependencies[1].atom_number = 0;
        atom[1].atom_number = 2;
        atom[1].flags = 1;
        atom[1].dependencies[0].atom_number = 1;
        atom[1].dependencies[0].type = 5;
        atom[1].dependencies[1].atom_number = 0;

        magma::Status status = ctx->ExecuteImmediateCommands(sizeof(atom), &atom, 0, nullptr);
        EXPECT_EQ(MAGMA_STATUS_CONTEXT_KILLED, status.get());
    }

    void TestInvalidSemaphoreImmediate()
    {
        auto ctx = InitializeContext();
        ASSERT_TRUE(ctx);

        magma_arm_mali_atom atom;
        atom.atom_number = 0;
        atom.flags = kAtomFlagSemaphoreSet;
        atom.dependencies[0].atom_number = 0;
        atom.dependencies[1].atom_number = 0;

        magma::Status status = ctx->ExecuteImmediateCommands(sizeof(atom), &atom, 0, nullptr);
        EXPECT_EQ(MAGMA_STATUS_CONTEXT_KILLED, status.get());
    }

    void TestSemaphoreImmediate()
    {
        auto ctx = InitializeContext();
        ASSERT_TRUE(ctx);
        auto platform_semaphore = magma::PlatformSemaphore::Create();
        uint32_t handle;
        platform_semaphore->duplicate_handle(&handle);
        connection_->ImportObject(handle, magma::PlatformObject::SEMAPHORE);

        magma_arm_mali_atom atom;
        atom.atom_number = 0;
        atom.flags = kAtomFlagSemaphoreSet;
        atom.dependencies[0].atom_number = 0;
        atom.dependencies[1].atom_number = 0;
        uint64_t semaphores[] = {platform_semaphore->id()};

        magma::Status status = ctx->ExecuteImmediateCommands(sizeof(atom), &atom, 1, semaphores);
        EXPECT_EQ(MAGMA_STATUS_OK, status.get());
    }

private:
    MagmaSystemContext* InitializeContext()
    {
        msd_drv_ = msd_driver_unique_ptr_t(msd_driver_create(), &msd_driver_destroy);
        if (!msd_drv_)
            return DRETP(nullptr, "failed to create msd driver");

        msd_driver_configure(msd_drv_.get(), MSD_DRIVER_CONFIG_TEST_NO_DEVICE_THREAD);

        platform_device_ = TestPlatformDevice::GetInstance();
        if (!platform_device_)
            DLOG("TestCommandBuffer: No platform device");
        auto msd_dev = msd_driver_create_device(
            msd_drv_.get(), platform_device_ ? platform_device_->GetDeviceHandle() : nullptr);
        if (!msd_dev)
            return DRETP(nullptr, "failed to create msd device");
        system_dev_ = std::shared_ptr<MagmaSystemDevice>(
            MagmaSystemDevice::Create(MsdDeviceUniquePtr(msd_dev)));
        uint32_t ctx_id = 0;
        auto msd_connection_t = msd_device_open(msd_dev, 0);
        if (!msd_connection_t)
            return DRETP(nullptr, "msd_device_open failed");
        connection_ = std::unique_ptr<MagmaSystemConnection>(new MagmaSystemConnection(
            system_dev_, MsdConnectionUniquePtr(msd_connection_t), MAGMA_CAPABILITY_RENDERING));
        if (!connection_)
            return DRETP(nullptr, "failed to connect to msd device");
        connection_->CreateContext(ctx_id);
        auto ctx = connection_->LookupContext(ctx_id);
        if (!ctx)
            return DRETP(nullptr, "failed to create context");
        return ctx;
    }

    msd_driver_unique_ptr_t msd_drv_;
    magma::PlatformDevice* platform_device_;
    std::shared_ptr<MagmaSystemDevice> system_dev_;
    std::unique_ptr<MagmaSystemConnection> connection_;
};

TEST(CommandBuffer, TestInvalidSemaphoreImmediate) { ::Test().TestInvalidSemaphoreImmediate(); }
TEST(CommandBuffer, TestSemaphoreImmediate) { ::Test().TestSemaphoreImmediate(); }
TEST(CommandBuffer, TestValidImmediate) { ::Test().TestValidImmediate(); }
TEST(CommandBuffer, TestInvalidInUse) { ::Test().TestInvalidInUse(); }
TEST(CommandBuffer, TestInvalidDependencyType) { ::Test().TestInvalidDependencyType(); }
TEST(CommandBuffer, TestInvalidDependencyNotSubmitted)
{
    ::Test().TestInvalidDependencyNotSubmitted();
}
}
