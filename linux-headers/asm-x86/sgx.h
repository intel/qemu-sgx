/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/**
 * Copyright(c) 2016-18 Intel Corporation.
 */
#ifndef _UAPI_ASM_X86_SGX_H
#define _UAPI_ASM_X86_SGX_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define SGX_MAGIC 0xA4

#define SGX_VIRT_EPC_CREATE \
	_IOW(SGX_MAGIC, 0x80, struct sgx_virt_epc_create)

/**
 * struct sgx_virt_epc_create - parameter structure for the
 *				%SGX_VIRT_EPC_CREATE ioctl
 *
 * @size:		size, in bytes, of the virtual EPC
 * @attribute_fd:	file handle to the securityfs attribute file
 */
struct sgx_virt_epc_create  {
	__u64	size;
	__u64	attribute_fd;
};

#endif /* _UAPI_ASM_X86_SGX_H */
