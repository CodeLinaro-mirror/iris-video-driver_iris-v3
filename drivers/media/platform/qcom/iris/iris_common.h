/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __IRIS_COMMON_H__
#define __IRIS_COMMON_H__

struct iris_inst;

int iris_process_streamon_input(struct iris_inst *inst);
int iris_process_streamon_output(struct iris_inst *inst);
int iris_session_streamoff(struct iris_inst *inst, u32 plane);

#endif
