Master
======
**DO NOT** use *master* if you want a stable build!  The *master* branch tracks the current state of development and will be rebased relatively frequently, e.g. on a weekly basis.  Using a tagged release (see below) is recommended for most users.

Releases
========
  * sgx-v2.11.1-r2
      - Add user control of leaf 0x12 SGX feature bits, e.g. SGX2, SGX_ENCLV, etc...
      - Update README to clarify -cpu usage

  * sgx-v2.11.1-r1
      - Rebase to upstream release v2.11.1

  * sgx-v2.10.1-r1
      - Change KVM_CAP_X86_VIRTUAL_EPC to 200 to avoid additional changes in the near future.

  * sgx-v2.10.1-alpha
      - Initial release (previously was the master branch)
      - KVM_CAP_X86_VIRTUAL_EPC is 150 in this release!  As is, this Qemu will only work with kernel release sgx-v4.14.0-alpha.

Introduction
============

## About

Intel® Software Guard Extensions (Intel® SGX) is an Intel technology designed to increase the security of application code and data.  This repository hosts preliminary Qemu patches to support SGX virtualization on KVM.  Because SGX is built on hardware features that cannot be emulated in software, virtualizing SGX requires support in KVM and in the host kernel.  The requesite Linux and KVM changes can be found in the kvm-sgx repository linked below.

  - [KVM Repository](https://github.com/intel/kvm-sgx)
  - [In-Kernel Linux Driver](https://github.com/jsakkine-intel/linux-sgx.git)
  - [Out-of-Kernel Linux Driver](https://github.com/intel/linux-sgx-driver)
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

By default, Qemu does not assign EPC to a VM, i.e. fully enabling SGX in a VM requires explicit allocation of EPC to the VM.  The size and location of the virtual EPC is controlled by the *-machine* options *epc* and *epc-below-4g* respectively.   

`-machine epc=<size>` defines the size of the guest's SGX virtual EPC, required for running SGX enclaves in the guest.  Enabling virtual EPC (size>0) requires SGX hardware and KVM support, i.e. KVM_CAP_X86_VIRTUAL_EPC, and will cause Qemu to fail if the necessary support is not available.  The default is 0.

`-machine epc-below-4g=<on|off|auto>` controls the location of the SGX virtual EPC in the physical address space.  On forces the EPC to be located below 4g (e.g. between RAM and MCFG).  Off forces the EPC to be located above 4g (e.g. between RAM and memory hotplug base).  Auto attempts to locate the EPC below the 4g boundary and falls back to above 4g if necessary, e.g. EPC cannot fit below 4g.  Qemu will fail if EPC cannot be placed in the requested location.  Default is auto.

Note: The size and location of the virtual EPC are far less restricted compared to physical EPC.  Because physical EPC is protected via range registers, the size of the physical EPC must be a power of two (though software sees a subset of the full EPC, e.g. 92M or 128M) and the EPC must be naturally aligned.  KVM SGX's virtual EPC is purely a software construct and only requires the size and location to be page aligned.  Qemu enforces the EPC size is a multiple of 4k and will ensure the base of the EPC is 4k aligned.

## Migration

Migration of SGX enabled VMs is allowed, but because EPC memory is encrypted with an ephemereal key that is tied to the underlying hardware, migrating a VM will result in the loss of EPC data, similar to loss of EPC data on system suspend.

Because KVM SGX reserves EPC pages at VM creation, migration will fail if the target system does not have sufficient EPC memory available for the VM, even if the VM is not actively using SGX.

## Launch Control

Qemu SGX support for Launch Control (LC) is passive, in the sense that it does not actively change the LC configuration.  Qemu SGX provides the user the ability to set/clear the CPUID flag (and by extension the associated IA32_FEATURE_CONTROL MSR bit in fw_cfg) and saves/restores the LE Hash MSRs when getting/putting guest state, but Qemu does not add new controls to directly modify the LC configuration.  Similar to hardware behavior, locking the LC configuration to a non-Intel value is left to guest firmware.

## Feature Control

Qemu SGX updates the `etc/msr_feature_control` fw_cfg entry to set the SGX (bit 18) and SGX LC (bit 17) flags based on their respective CPUID support, i.e. existing guest firmware will automatically set SGX and SGX LC accordingly, assuming said firmware supports fw_cfg.msr_feature_control.