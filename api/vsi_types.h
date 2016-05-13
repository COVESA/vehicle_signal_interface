/* Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma
#ifndef _VSI_TYPES_H_
#define _VSI_TYPES_H_

/**
 * Signal identifier abstraction designed for dealing with different domains of
 * signals.
 */
union vsi_signal_id {
    /** Structure representing the parts of a full signal identifier. */
    struct {
        /** The domain ID for the signal. */
        unsigned int domain_id;
        /** The signal ID for the signal. */
        unsigned int signal_id;
    } parts;
    /** Combined domain+signal ID into single ID for simplicity. */
    unsigned long full_id;
};

/**
 * Used to keep track of what groups have been registered by the running
 * application.
 */
struct vsi_group_list_entry {
    /** The group ID. */
    unsigned long group;
    /** Pointer to the next container for a group ID. */
    struct vsi_group_list_entry *next;
};

/** Contains the core information for a signal from an application. */
struct vsi_signal {
    /** The signal identifier for the specified signal. */
    union vsi_signal_id signal_id;
    /** The group ID this signal belongs to, if any. */
    unsigned long group_id;
    /** The core semaphore structure used by the lower levels. */
    sem_t __sem;
};

/**
 * List used to keep track of what signals have been subscribed by the
 * application.
 */
struct vsi_signal_list_entry {
    /** Current signal entry. */
    struct vsi_signal signal;
    /** Pointer to the next container for signal information. */
    struct vsi_signal_list_entry *next;
};

/** Master structure that gets passed to all VSI functions. */
struct vsi_context {
    /** List of current groups. */
    struct vsi_group_list_entry *group_head;
    /** List of current signals. */
    struct vsi_signal_list_entry *signal_head;
};

#endif // _VSI_TYPES_H_
