/*
 * QEMU host SGX EPC memory backend
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * Authors:
 *   Sean Christopherson <sean.j.christopherson@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include <sys/ioctl.h>

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qom/object_interfaces.h"
#include "qapi/error.h"
#include "sysemu/hostmem.h"
#include "hw/i386/pc.h"

#define TYPE_MEMORY_BACKEND_EPC "memory-backend-epc"

#define MEMORY_BACKEND_EPC(obj)                                        \
    OBJECT_CHECK(HostMemoryBackendEpc, (obj), TYPE_MEMORY_BACKEND_EPC)

typedef struct HostMemoryBackendEpc HostMemoryBackendEpc;

struct HostMemoryBackendEpc {
    HostMemoryBackend parent_obj;
};

static void
sgx_epc_backend_memory_alloc(HostMemoryBackend *backend, Error **errp)
{
    char *name;
    int fd;

    if (!backend->size) {
        error_setg(errp, "can't create backend with size 0");
        return;
    }

    fd = open("/dev/sgx_virt_epc", O_RDWR);
    if (fd < 0) {
        error_setg_errno(errp, errno,
                         "failed to open /dev/sgx_virt_epc to alloc SGX EPC");
        return;
    }

    name = object_get_canonical_path(OBJECT(backend));
    memory_region_init_ram_from_fd(&backend->mr, OBJECT(backend),
                                   name, backend->size, backend->share, true,
                                   fd, errp);
    g_free(name);
}

void sgx_memory_backend_reset(HostMemoryBackend *backend, int fd,
                              Error **errp)
{
    MemoryRegion *mr = &backend->mr;

    mr->enabled = false;

    // Destroy the old memory region if it exist.
    if (fd > 0 && mr->destructor) {
        mr->destructor(mr);
        close(fd);
    }

    sgx_epc_backend_memory_alloc(backend, errp);
}

static void sgx_epc_backend_instance_init(Object *obj)
{
    HostMemoryBackend *m = MEMORY_BACKEND(obj);

    m->share = true;
    m->merge = false;
    m->dump = false;
}

static void sgx_epc_backend_class_init(ObjectClass *oc, void *data)
{
    HostMemoryBackendClass *bc = MEMORY_BACKEND_CLASS(oc);

    bc->alloc = sgx_epc_backend_memory_alloc;
}

static const TypeInfo sgx_epc_backed_info = {
    .name = TYPE_MEMORY_BACKEND_EPC,
    .parent = TYPE_MEMORY_BACKEND,
    .instance_init = sgx_epc_backend_instance_init,
    .class_init = sgx_epc_backend_class_init,
    .instance_size = sizeof(HostMemoryBackendEpc),
};

static void register_types(void)
{
    int fd = open("/dev/sgx_virt_epc", O_RDWR);
    if (fd >= 0) {
        close(fd);

        type_register_static(&sgx_epc_backed_info);
    }
}

type_init(register_types);
