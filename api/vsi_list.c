/* Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <stdlib.h>
#include "vsi_list.h"

//
// TODO: Add group support, which has been intentionally left out at this point
//       to more rapidly give interested parties a functioning piece of code to
//       play with.
//

int vsi_list_insert(struct vsi_context *context, unsigned int domain_id,
                    unsigned int signal_id)
{
    struct vsi_signal_list_entry *new_entry, *curr;
    int err;

    if (!context)
        return -EINVAL;

    // Create the new entry for the list.
    new_entry = malloc(sizeof(struct vsi_signal_list_entry));
    if (!new_entry)
        return -ENOMEM;

    // TODO: Deal with semaphore ID conflicts.
    err = sem_init(&new_entry->signal.__sem, 1, signal_id);
    if (err) {
        free(new_entry);
        return err;
    }

    new_entry->signal.signal_id.parts.domain_id = domain_id;
    new_entry->signal.signal_id.parts.signal_id = signal_id;
    new_entry->signal.group_id = 0;
    new_entry->next = NULL;

    // If this is the first entry, initialize the list.
    if (!context->signal_head)
    {
        context->signal_head = new_entry;
        return 0;
    }

    // TODO: Error on duplicate entries.

    // Find the last entry in the list and insert this entry after it.
    for (curr = context->signal_head; curr->next; curr = curr->next);
    curr->next = new_entry;

    return 0;
}

int vsi_list_remove(struct vsi_context *context, unsigned int domain_id,
                    unsigned int signal_id)
{
    struct vsi_signal_list_entry *curr, *prev = NULL;
    union vsi_signal_id id;
    int err;

    if (!context || !context->signal_head)
        return -EINVAL;

    // For comparison later.
    id.parts.domain_id = domain_id;
    id.parts.signal_id = signal_id;

    // Find the match.
    for (curr = context->signal_head; curr &&
         curr->signal.signal_id.full_id != id.full_id; curr = curr->next)
    {
        // Keep track of the last enry in the list.
        prev = curr;
    }

    // If we could not find the entry to remove, error out.
    if (!curr)
        return -ENOENT;

    err = sem_destroy(&curr->signal.__sem);
    if (err)
        return -EBUSY;

    // Drop the entry from the list.
    if (prev)
        prev->next = curr->next;
    else if (curr->next)
        context->signal_head = curr->next;
    else
        context->signal_head = NULL;
    free(curr);

    return 0;
}
