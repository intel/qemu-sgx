/*
 * SGX EPC device
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * Authors:
 *   Sean Christopherson <sean.j.christopherson@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "hw/i386/pc.h"
#include "hw/i386/sgx-epc.h"
#include "hw/mem/memory-device.h"
#include "monitor/qdev.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/config-file.h"
#include "qemu/error-report.h"
#include "qemu/option.h"
#include "qemu/units.h"
#include "target/i386/cpu.h"

static Property sgx_epc_properties[] = {
    DEFINE_PROP_UINT64(SGX_EPC_ADDR_PROP, SGXEPCDevice, addr, 0),
    DEFINE_PROP_LINK(SGX_EPC_MEMDEV_PROP, SGXEPCDevice, hostmem,
                     TYPE_MEMORY_BACKEND, HostMemoryBackend *),
    DEFINE_PROP_END_OF_LIST(),
};

static void sgx_epc_get_size(Object *obj, Visitor *v, const char *name,
                             void *opaque, Error **errp)
{
    Error *local_err = NULL;
    uint64_t value;

    value = memory_device_get_region_size(MEMORY_DEVICE(obj), &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    visit_type_uint64(v, name, &value, errp);
}

static void sgx_epc_init(Object *obj)
{
    object_property_add(obj, SGX_EPC_SIZE_PROP, "uint64", sgx_epc_get_size,
                        NULL, NULL, NULL, &error_abort);
}

static void sgx_epc_realize(DeviceState *dev, Error **errp)
{
    PCMachineState *pcms = PC_MACHINE(qdev_get_machine());
    MemoryDeviceState *md = MEMORY_DEVICE(dev);
    SGXEPCState *sgx_epc = pcms->sgx_epc;
    SGXEPCDevice *epc = SGX_EPC(dev);

    if (pcms->boot_cpus != 0) {
        error_setg(errp,
            "'" TYPE_SGX_EPC "' can't be created after vCPUs, e.g. via -device");
        return;
    }

    if (!epc->hostmem) {
        error_setg(errp, "'" SGX_EPC_MEMDEV_PROP "' property is not set");
        return;
    } else if (host_memory_backend_is_mapped(epc->hostmem)) {
        char *path = object_get_canonical_path_component(OBJECT(epc->hostmem));
        error_setg(errp, "can't use already busy memdev: %s", path);
        g_free(path);
        return;
    }

    epc->addr = sgx_epc->base + sgx_epc->size;

    memory_region_add_subregion(&sgx_epc->mr, epc->addr - sgx_epc->base,
                                host_memory_backend_get_memory(epc->hostmem));

    host_memory_backend_set_mapped(epc->hostmem, true);

    sgx_epc->sections = g_renew(SGXEPCDevice *, sgx_epc->sections,
                                sgx_epc->nr_sections + 1);
    sgx_epc->sections[sgx_epc->nr_sections++] = epc;

    sgx_epc->size += memory_device_get_region_size(md, errp);
}

static void sgx_epc_unrealize(DeviceState *dev, Error **errp)
{
    SGXEPCDevice *epc = SGX_EPC(dev);

    host_memory_backend_set_mapped(epc->hostmem, false);
}

static uint64_t sgx_epc_md_get_addr(const MemoryDeviceState *md)
{
    const SGXEPCDevice *epc = SGX_EPC(md);

    return epc->addr;
}

static void sgx_epc_md_set_addr(MemoryDeviceState *md, uint64_t addr,
                                Error **errp)
{
    object_property_set_uint(OBJECT(md), addr, SGX_EPC_ADDR_PROP, errp);
}

static uint64_t sgx_epc_md_get_plugged_size(const MemoryDeviceState *md,
                                            Error **errp)
{
    return 0;
}

static MemoryRegion *sgx_epc_md_get_memory_region(MemoryDeviceState *md,
                                                  Error **errp)
{
    SGXEPCDevice *epc = SGX_EPC(md);

    if (!epc->hostmem) {
        error_setg(errp, "'" SGX_EPC_MEMDEV_PROP "' property must be set");
        return NULL;
    }

    return host_memory_backend_get_memory(epc->hostmem);
}

static void sgx_epc_md_fill_device_info(const MemoryDeviceState *md,
                                        MemoryDeviceInfo *info)
{
    // TODO
}

static void sgx_epc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    MemoryDeviceClass *mdc = MEMORY_DEVICE_CLASS(oc);

    dc->hotpluggable = false;
    dc->realize = sgx_epc_realize;
    dc->unrealize = sgx_epc_unrealize;
    dc->props = sgx_epc_properties;
    dc->desc = "SGX EPC section";

    mdc->get_addr = sgx_epc_md_get_addr;
    mdc->set_addr = sgx_epc_md_set_addr;
    mdc->get_plugged_size = sgx_epc_md_get_plugged_size;
    mdc->get_memory_region = sgx_epc_md_get_memory_region;
    mdc->fill_device_info = sgx_epc_md_fill_device_info;
}

static TypeInfo sgx_epc_info = {
    .name          = TYPE_SGX_EPC,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(SGXEPCDevice),
    .instance_init = sgx_epc_init,
    .class_init    = sgx_epc_class_init,
    .class_size    = sizeof(DeviceClass),
    .interfaces = (InterfaceInfo[]) {
        { TYPE_MEMORY_DEVICE },
        { }
    },
};

static void sgx_epc_register_types(void)
{
    type_register_static(&sgx_epc_info);
}

type_init(sgx_epc_register_types)

int sgx_epc_get_section(int section_nr, uint64_t *addr, uint64_t *size)
{
    PCMachineState *pcms = PC_MACHINE(qdev_get_machine());
    SGXEPCDevice *epc;

    if (pcms->sgx_epc == NULL || pcms->sgx_epc->nr_sections <= section_nr) {
        return 1;
    }

    epc = pcms->sgx_epc->sections[section_nr];

    *addr = epc->addr;
    *size = memory_device_get_region_size(MEMORY_DEVICE(epc), &error_fatal);

    return 0;
}


static int sgx_epc_set_property(void *opaque, const char *name,
                                const char *value, Error **errp)
{
    Object *obj = opaque;
    Error *err = NULL;

    object_property_parse(obj, value, name, &err);
    if (err != NULL) {
        error_propagate(errp, err);
        return -1;
    }
    return 0;
}

static int sgx_epc_init_func(void *opaque, QemuOpts *opts, Error **errp)
{
    Error *err = NULL;
    Object *obj;

    obj = object_new("sgx-epc");

    qdev_set_id(DEVICE(obj), qemu_opts_id(opts));

    if (qemu_opt_foreach(opts, sgx_epc_set_property, obj, &err)) {
        goto out;
    }

    object_property_set_bool(obj, true, "realized", &err);

out:
    if (err != NULL) {
        error_propagate(errp, err);
    }
    object_unref(obj);
    return err != NULL ? -1 : 0;
}

void pc_machine_init_sgx_epc(PCMachineState *pcms)
{
    SGXEPCState *sgx_epc;

    if (!sgx_epc_enabled) {
        return;
    }

    sgx_epc = g_malloc0(sizeof(*sgx_epc));
    pcms->sgx_epc = sgx_epc;

    sgx_epc->base = 0x100000000ULL + pcms->above_4g_mem_size;

    memory_region_init(&sgx_epc->mr, OBJECT(pcms), "sgx-epc", UINT64_MAX);
    memory_region_add_subregion(get_system_memory(), sgx_epc->base,
                                &sgx_epc->mr);

    qemu_opts_foreach(qemu_find_opts("sgx-epc"), sgx_epc_init_func, NULL,
                      &error_fatal);

    if ((sgx_epc->base + sgx_epc->size) < sgx_epc->base) {
        error_report("Size of all 'sgx-epc' =0x%"PRIu64" causes EPC to wrap",
                     sgx_epc->size);
        exit(EXIT_FAILURE);
    }

    memory_region_set_size(&sgx_epc->mr, sgx_epc->size);
}

static QemuOptsList sgx_epc_opts = {
    .name = "sgx-epc",
    .implied_opt_name = "id",
    .head = QTAILQ_HEAD_INITIALIZER(sgx_epc_opts.head),
    .desc = {
        {
            .name = "id",
            .type = QEMU_OPT_STRING,
            .help = "SGX EPC section ID",
        },{
            .name = "memdev",
            .type = QEMU_OPT_STRING,
            .help = "memory object backend",
        },
        { /* end of list */ }
    },
};

static void sgx_epc_register_opts(void)
{
    qemu_add_opts(&sgx_epc_opts);
}

opts_init(sgx_epc_register_opts);
