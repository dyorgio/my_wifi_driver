/*
 * fw_download.h
 *
 * used for firmware download
 *
 * Author: hejiuyi
 *
 * Copyright (c) 2020 SmartChip Integrated Circuits(SuZhou ZhongKe) Co.,Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __FW_DOWNLAOD_H__
#define __FW_DOWNLAOD_H__

#pragma pack(1)
typedef struct fw_file_header
{
    wf_u16 magic_number;
    wf_u16 interface_type;
    wf_u8  rom_type;
    wf_u8  firmware_num;
} fw_file_header_t;

typedef struct fw_header
{
    wf_u8 type;
    wf_u8 sub_type;
    wf_u32 version;
    wf_u32 length;
    wf_u32 offset;
} fw_header_t;
#pragma pack()

int wf_fw_download(nic_info_st *nic_info);

#endif

