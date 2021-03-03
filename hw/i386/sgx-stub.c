#include "qemu/osdep.h"
#include "hw/i386/pc.h"
#include "hw/i386/sgx-epc.h"

void pc_machine_init_sgx_epc(PCMachineState *pcms)
{
    return;
}

int sgx_epc_get_section(int section_nr, uint64_t *addr, uint64_t *size)
{
    return 1;
}
