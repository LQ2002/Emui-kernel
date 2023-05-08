/* utils.h
 *
 * utils interface
 *
 * Copyright (c) 2019-2019 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef UTILS__H
#define UTILS__H

#include "phys_mem.h"

extern unsigned long virt_to_phy_u(unsigned long vaddr);
extern int write_hisi_nve_ddrfault(struct user_nve_info *request);

#endif
