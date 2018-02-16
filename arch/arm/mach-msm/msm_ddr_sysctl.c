/* linux/arch/arm/mach-msm/msm_ddr_sysctl.c
 *
 * Copyright 2017-2018 Fairphone B.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/sysctl.h>
#include <mach/msm_smem.h>
#include "smem_private.h"


/* DDR vendor identifiers (64-bits unsigned integer) */
enum {
    DDR_VENDOR_ID_INVALID   = -1,
    DDR_VENDOR_ID_SAMSUNG   = 1,
    DDR_VENDOR_ID_MICRON    = 255,
};

/* Maximum DDR vendor name length */
#define DDR_VENDOR_NAME_MAX_LEN 32

/* DDR vendor names (shorter than DDR_VENDOR_NAME_MAX_LEN) */
char *DDR_VENDOR_NAME_MICRON    = "Micron";
char *DDR_VENDOR_NAME_SAMSUNG   = "Samsung";
char *DDR_VENDOR_NAME_UNKNOWN   = "Unknown";

static char ddr_vendor[DDR_VENDOR_NAME_MAX_LEN] = "";

static struct ctl_table_header *ddr_table_header;

static ctl_table ddr_vendor_table[] = {
    {
        .procname       = "vendor",
        .data           = ddr_vendor,
        .maxlen         = DDR_VENDOR_NAME_MAX_LEN,
        .mode           = 0444,
        .proc_handler   = proc_dostring,
    },
    {}
};

static ctl_table ddr_dir_table[] = {
    {
        .procname       = "ddr",
        .mode           = 0555,
        .child          = ddr_vendor_table,
    },
    {}
};

static ctl_table dev_root_table[] = {
    {
        .procname       = "dev",
        .mode           = 0555,
        .child          = ddr_dir_table,
    },
    {}
};


static int get_ddr_vendor_id_from_smem(void) {
    unsigned int smem_size;
    unsigned int *smem_ddr_vendor_id;

    smem_ddr_vendor_id = smem_get_entry(SMEM_DDR_VENDOR_ID, &smem_size);
    if (smem_ddr_vendor_id == NULL) {
        printk("Could not get the DDR vendor identifier from SMEM\n");
        return DDR_VENDOR_ID_INVALID;
    }

    return (int) *smem_ddr_vendor_id;
}

static int __init msm_ddr_init(void) {
    switch (get_ddr_vendor_id_from_smem()) {
        case DDR_VENDOR_ID_MICRON:
            strncpy(ddr_vendor, DDR_VENDOR_NAME_MICRON, DDR_VENDOR_NAME_MAX_LEN);
            break;
        case DDR_VENDOR_ID_SAMSUNG:
            strncpy(ddr_vendor, DDR_VENDOR_NAME_SAMSUNG, DDR_VENDOR_NAME_MAX_LEN);
            break;
        case DDR_VENDOR_ID_INVALID:
        default:
            strncpy(ddr_vendor, DDR_VENDOR_NAME_UNKNOWN, DDR_VENDOR_NAME_MAX_LEN);
            break;
    }
    ddr_vendor[DDR_VENDOR_NAME_MAX_LEN-1] = '\0';

    ddr_table_header = register_sysctl_table(dev_root_table);
    if (!ddr_table_header)
        return -ENOMEM;

    return 0;
}

static void __exit msm_ddr_exit(void) {
    unregister_sysctl_table(ddr_table_header);
}

module_init(msm_ddr_init);
module_exit(msm_ddr_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DDR information");
