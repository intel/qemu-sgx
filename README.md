Master
======
The *master* branch generally tracks the most recent release and will occasionally be rebased without warning.  Using a [tagged release](#releases) is recommended for most users.

Introduction
============

## About

Intel® Software Guard Extensions (Intel® SGX) is an Intel technology designed to increase the security of application code and data.  This repository hosts preliminary Qemu patches to support SGX virtualization on KVM.  Because SGX is built on hardware features that cannot be emulated in software, virtualizing SGX requires support in KVM and in the host kernel.  The requesite Linux and KVM changes can be found in the [KVM SGX Tree](https://github.com/intel/kvm-sgx).

Utilizing SGX in the guest requires a kernel/OS with SGX support, e.g. a kernel buit using the [SGX Linux Development Tree](https://github.com/jsakkine-intel/linux-sgx.git) or the [KVM SGX Tree](https://github.com/intel/kvm-sgx).  Running KVM SGX as the guest kernel allows nested virtualization of SGX (obviously requires a compatible Qemu build).  An alternative option to recompiling your guest kernel is to use the out-of-tree SGX driver (compiles as a kernel module) on top of a non-SGX kernel, e.g. your distro's standard kernel.

  - [KVM SGX Tree](https://github.com/intel/kvm-sgx)
  - [SGX Linux Development Tree](https://github.com/jsakkine-intel/linux-sgx.git)
  - [SGX Out-of-Tree Driver for Linux](https://github.com/intel/linux-sgx-driver)
  - [SGX Homepage](https://software.intel.com/sgx)
  - [SGX SDK](https://software.intel.com/sgx-sdk)
  - [Linux SDK Downloads](https://01.org/intel-software-guard-extensions/downloads)

## Caveat Emptor

SGX virtualization on Qemu/KVM is under active development.  The patches in this repository are intended for experimental purposes only, they are not mature enough for production use.

This readme assumes that the reader is familiar with Intel® SGX, Qemu, KVM and Linux, and has experience building Qemu and the Linux kernel.  Only the aspects of SGX virtualization directly related to Qemu are covered, please see the kvm-sgx repository for additional details on virtualizing SGX.


Qemu SGX
========

## Enabling

Due to its myriad dependencies, SGX is currently not listed as supported in any of Qemu's built-in CPU configuration.  To expose SGX (and SGX Launch Control) to a guest, you must either use `-cpu host` to pass-through the host CPU model, or explicitly enable SGX when using a built-in CPU model, e.g. via `-cpu <model>,+sgx` or `-cpu <model>,+sgx,+sgxlc`.

## Virtual EPC

By default, Qemu does not assign EPC to a VM, i.e. fully enabling SGX in a VM requires explicit allocation of EPC to the VM.  Similar to other specialized memory types, e.g. hugetlbfs, EPC is exposed as a memory backend.  For a number of reasons, a EPC memory backend can only be realized via an 'sgx-epc' device.  Standard memory backend options such as prealloc are supported by EPC.

SGX EPC is enumerated through CPUID, i.e. EPC "devices" need to be realized prior to realizing the vCPUs themselves, which occurs long before generic devices are parsed and realized.  Because of this, 'sgx-epc' devices must be created via the dedicated -sgx-epc command, i.e. cannot be created through the generic -devices command.  On the plus side, this limitation means that EPC does not require -maxmem as EPC is not treated as {cold,hot}plugged memory.

Qemu does not artificially restrict the number of EPC sections exposed to a guest, e.g. Qemu will happily allow you to create 64 1M EPC sections.  Be aware that some kernels may not recognize all EPC sections, e.g. the Linux SGX driver is hardwired to support only 8 EPC sections.

The following Qemu snippet creates two EPC sections, with 64M pre-allocated to the VM and an additional 28M mapped but not allocated:

`-object memory-backend-epc,id=mem1,size=64M,prealloc -sgx-epc id=epc1,memdev=mem1 -object memory-backend-epc,id=mem2,size=28M -sgx-epc id=epc2,memdev=mem2`

Note: The size and location of the virtual EPC are far less restricted compared to physical EPC.  Because physical EPC is protected via range registers, the size of the physical EPC must be a power of two (though software sees a subset of the full EPC, e.g. 92M or 128M) and the EPC must be naturally aligned.  KVM SGX's virtual EPC is purely a software construct and only requires the size and location to be page aligned.  Qemu enforces the EPC size is a multiple of 4k and will ensure the base of the EPC is 4k aligned.  To simplify the implementation, EPC is always located above 4g in the guest physical address space.

## Migration

Migration of SGX enabled VMs is allowed, but because EPC memory is encrypted with an ephemereal key that is tied to the underlying hardware, migrating a VM will result in the loss of EPC data, similar to loss of EPC data on system suspend.

## CPUID

All SGX sub-features enumerated through CPUID, e.g. SGX2, MISCSELECT, ATTRIBUTES, etc... can be restricted via CPUID flags.  Be aware that enforcing restriction of MISCSELECT, ATTRIBUTES and XFRM requires intercepting ECREATE, i.e. may marginally reduce SGX performance in the guest.  All SGX sub-features controlled via -cpu are prefixed with "sgx", e.g.:

```
$ qemu-system-x86_64 -cpu help | xargs printf "%s\n" | grep sgx
sgx
sgx-debug
sgx-encls-c
sgx-enclv
sgx-exinfo
sgx-kss
sgx-mode64
sgx-provisionkey
sgx-tokenkey
sgx1
sgx2
sgxlc
```

The following Qemu snippet passes through the host CPU (and host physical address width) but restricts access to the provision and EINIT token keys:

`-cpu host,host-phys-bits,-sgx-provisionkey,-sgx-tokenkey`

Note: SGX sub-features cannot be emulated, i.e. sub-features that are not present in hardware cannot be forced on via '-cpu'.

## Launch Control

Qemu SGX support for Launch Control (LC) is passive, in the sense that it does not actively change the LC configuration.  Qemu SGX provides the user the ability to set/clear the CPUID flag (and by extension the associated IA32_FEATURE_CONTROL MSR bit in fw_cfg) and saves/restores the LE Hash MSRs when getting/putting guest state, but Qemu does not add new controls to directly modify the LC configuration.  Similar to hardware behavior, locking the LC configuration to a non-Intel value is left to guest firmware.

## Feature Control

Qemu SGX updates the `etc/msr_feature_control` fw_cfg entry to set the SGX (bit 18) and SGX LC (bit 17) flags based on their respective CPUID support, i.e. existing guest firmware will automatically set SGX and SGX LC accordingly, assuming said firmware supports fw_cfg.msr_feature_control.

Releases
========
  * sgx-v3.1.0-r1
      - Rebase to upstream release v3.1.0-r1
      - Expose EPC via memory backend and sgx-epc device
      - Support restricting SGX sub-features via CPUID flags
      - Compatible with KVM SGX kernel release sgx-v5.0.0-r1

  * sgx-v3.0.0-r2
      - Set virtual EPC using KVM_SET_USER_MEMORY_REGION with KVM_MEM_SGX_EPC flag
      - Compatible with KVM SGX kernel release sgx-v4.18.3-r2 and sgx-v4.19.1-r1

  * sgx-v3.0.0-r1
      - Rebase to upstream release v3.0.0
      - Set virtual EPC using new ioctl KVM_X86_SET_SGX_EPC
      - Compatible with KVM SGX kernel release sgx-v4.18.3-r1

  * sgx-v2.11.1-r2
      - Add user control of leaf 0x12 SGX feature bits, e.g. SGX2, SGX_ENCLV, etc...
      - Update README to clarify -cpu usage
      - Compatible with KVM SGX kernel releases sgx-v4.14.28-r1, sgx-v4.15.11-r1 and sgx-v4.17.3-r1

  * sgx-v2.11.1-r1
      - Rebase to upstream release v2.11.1
      - Compatible with KVM SGX kernel releases sgx-v4.14.28-r1, sgx-v4.15.11-r1 and sgx-v4.17.3-r1

  * sgx-v2.10.1-r1
      - Change KVM_CAP_X86_VIRTUAL_EPC to 200 to avoid additional changes in the near future.
      - Compatible with KVM SGX kernel releases sgx-v4.14.28-r1, sgx-v4.15.11-r1 and sgx-v4.17.3-r1

  * sgx-v2.10.1-alpha
      - Initial release (previously was the master branch)
      - KVM_CAP_X86_VIRTUAL_EPC is 150 in this release!  As is, this Qemu will only work with kernel release sgx-v4.14.0-alpha.