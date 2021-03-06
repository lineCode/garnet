/*
 * Copyright (c) 2012 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

//#include <linux/debugfs.h>
//#include <linux/devcoredump.h>
//#include <linux/module.h>
//#include <linux/netdevice.h>

#include "brcmu_utils.h"
#include "brcmu_wifi.h"
#include "bus.h"
#include "core.h"
#include "debug.h"
#include "device.h"
#include "linuxisms.h"
#include "fweh.h"

static struct dentry* root_folder;

zx_status_t brcmf_debug_create_memdump(struct brcmf_bus* bus, const void* data, size_t len) {
    void* dump;
    size_t ramsize;
    zx_status_t err;

    ramsize = brcmf_bus_get_ramsize(bus);
    if (!ramsize) {
        return ZX_ERR_NOT_SUPPORTED;
    }

    dump = vzalloc(len + ramsize);
    if (!dump) {
        return ZX_ERR_NO_MEMORY;
    }

    memcpy(dump, data, len);
    err = brcmf_bus_get_memdump(bus, dump + len, ramsize);
    if (err != ZX_OK) {
        vfree(dump);
        return err;
    }

    dev_coredumpv(bus->dev, dump, len + ramsize, GFP_KERNEL);

    return ZX_OK;
}

void brcmf_debugfs_init(void) {
    zx_status_t err;

    err = debugfs_create_dir(KBUILD_MODNAME, NULL, &root_folder);
    if (err != ZX_OK) {
        root_folder = NULL;
    }
}

void brcmf_debugfs_exit(void) {
    if (!root_folder) {
        return;
    }

    debugfs_remove_recursive(root_folder);
    root_folder = NULL;
}

zx_status_t brcmf_debug_attach(struct brcmf_pub* drvr) {
    struct brcmf_device* dev = drvr->bus_if->dev;
    zx_status_t ret;

    if (!root_folder) {
        return ZX_ERR_NOT_FILE;
    }

    ret = debugfs_create_dir(dev_name(dev), root_folder, &drvr->dbgfs_dir);
    return ret;
}

void brcmf_debug_detach(struct brcmf_pub* drvr) {
    brcmf_fweh_unregister(drvr, BRCMF_E_PSM_WATCHDOG);

    if (drvr->dbgfs_dir != NULL) {
        debugfs_remove_recursive(drvr->dbgfs_dir);
    }
}

struct dentry* brcmf_debugfs_get_devdir(struct brcmf_pub* drvr) {
    return drvr->dbgfs_dir;
}

zx_status_t brcmf_debugfs_add_entry(struct brcmf_pub* drvr, const char* fn,
                                    zx_status_t (*read_fn)(struct seq_file* seq, void* data)) {
    struct dentry* e;
    zx_status_t ret;

    ret = debugfs_create_devm_seqfile(drvr->bus_if->dev, fn, drvr->dbgfs_dir, read_fn, &e);
    return ret;
}
