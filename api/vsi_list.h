/* Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _VSI_LIST_H_
#define _VSI_LIST_H_

#include "vsi_types.h"

int vsi_list_insert(struct vsi_context *context, unsigned int domain_id,
                    unsigned int signal_id);
int vsi_list_remove(struct vsi_context *context, unsigned int domain_id,
                    unsigned int signal_id);

#endif // _VSI_LIST_H_
