# Debian Guest

The `debian_guest` package provides a more substantial Linux environment than
that provided by the `linux_guest` package.

## Build with Bundled Root Filesystem

These steps will walk through building a the package with the root filesystem
bundled as a package resource. The root filesystem will appear writable but
all writes are volatile and will disappear when the guest shuts down.

`Note:` these instructions assume an x86 device configured to use the boot paver.

`Note:` due to the size of the filesystem image, this method is not supported on
the VIM2.

```
$ cd $FUCHSIA_DIR
$ ./garnet/bin/guest/pkg/debian_guest/build-image.sh x86
$ fx set x64 --packages "garnet/packages/experimental/debian_guest"
$ fx full-build
$ fx boot
```

## Build with a Persistent Disk

To achieve a persistent disk, we'll build the root filesystem on a USB stick
that will be provided to the guest.

```
$ cd $FUCHSIA_DIR

# Insert the USB drive into your workstation. The build-usb.sh script will ask
# you for the disk name. Once the script completes, insert the USB stick into
# the target device.

# For VIM2
$ ./garnet/bin/guest/pkg/debian_guest/build-usb.sh arm64
$ fx set arm64 --packages "garnet/packages/experimental/debian_guest" --args "debian_guest_usb_root=true" --netboot
$ fx full-build
$ fx boot netboot

# Similarly for x86:
$ ./garnet/bin/guest/pkg/debian_guest/build-usb.sh x86
$ fx set x64 --packages "garnet/packages/experimental/debian_guest" --args "debian_guest_usb_root=true"
$ fx full-build
$ fx boot
```

## Running `debian_guest`

Once booted:

```
guest launch debian_guest
```