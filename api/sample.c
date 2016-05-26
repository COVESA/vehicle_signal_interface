/* Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "vsi.h"

int main()
{
    struct vsi_data my_data, *my_group_data[2];
    unsigned int domain_id, signal_id;
    unsigned long group_id;
    vsi_handle handle;
    void *my_buffer, *foo;
    int err;
    
    // Call into the API to initialize the memory.
    handle = vsi_initialize();
    if (!handle)
    {
        printf("Failed to allocate memory for VSI!\n");
        return -1;
    }
    printf("Allocated memory for VSI.\n");

    //
    // Pretend this application owns some signal named "foo" and that it wants
    // to lock the signal so no other application may generate "foo" signals.
    //
    // This call is optional.
    //
    err = vsi_register_signal_by_name(handle, "foo");
    if (err)
    {
        printf("Failed to register the signal \"foo\"! Error code %d.\n", err);
        return err;
    }
    printf("Took ownership of signal \"foo\".\n");

    // Fire a signal into the void to show how generating a signal works.
    err = vsi_name_string_to_id(handle, "foo", &domain_id, &signal_id);
    if (err)
    {
        printf("Failed to find the signal ID for the signal \"foo\"! Error code %d.\n",
               err);
        return err;
    }
    my_buffer = malloc(sizeof(unsigned char));
    if (!my_buffer)
    {
        printf("Failed to allocate memory for a signal buffer!\n");
        return -1;
    }
    *(unsigned char *)my_buffer = 42;
    err = vsi_fire_signal(handle, domain_id, signal_id, my_buffer,
                          sizeof(unsigned char));
    if (err)
    {
        printf("Failed to fire the signal \"foo\"! Error code %d.\n", err);
        return err;
    }
    free(my_buffer);
    printf("Successfully fired signal \"foo\".\n");

    // Initialize a buffer for data to go into.
    my_data.data = malloc(sizeof(unsigned char));
    my_data.len = sizeof(unsigned char);

    // Read the latest update from the "bar" signal.
    err = vsi_get_newest_signal_by_name(handle, "bar", &my_data);
    if (err)
    {
        printf("Failed to get the newest signal data for \"bar\"! Error code %d.\n",
               err);
        return err;
    }
    printf("Successfully read the \"bar\" signal and got %d.\n",
           *(unsigned char *)my_data.data);

    //
    // Go through the flow of subscribing to a signal and listening for it to
    // fire.
    //
    err = vsi_subscribe_to_signal_by_name(handle, "bar");
    if (err)
    {
        printf("Failed to subscribe to \"bar\"! Error code %d.\n", err);
        return err;
    }
    printf("Subscribed to signal \"bar\".\n");

    err = vsi_listen(handle, &domain_id, &signal_id, &group_id, 0);
    if (err)
    {
        printf("Failed to listen for signals! Error code %d.\n", err);
        return err;
    }
    printf("Listened and got an event!\n");

    //
    // NOTE: We know that "bar" fired as it was the only signal we subscribed
    //       to and we gave an infinite timeout to vsi_listen. Here is where the
    //       application would programmatically discover this.
    //
    do
    {
        err = vsi_get_oldest_signal_by_name(handle, "bar", &my_data);
        if (err && err != -ENODATA)
        {
            printf("Failed to get the oldest signal data for \"bar\"! Error code %d.\n",
                   err);
            return err;
        }
        printf("Successfully read the oldest \"bar\" signal and got %d.\n",
               *(unsigned char *)my_data.data);
    } while (err != -ENODATA);
    printf("Completed reading the signal.\n");

    //
    // NOTE: At this point, we could jump back to the vsi_listen call and repeat
    //       the cycle as appropriate.
    //

    //
    // Clean up the signal data buffer, since we no longer need it, and
    // unsubscribe from the "bar" signal.
    //
    free(my_data.data);
    err = vsi_unsubscribe_from_signal_by_name(handle, "bar");
    if (err)
    {
        printf("Failed to unsubscribe from \"bar\"! Error code %d.\n", err);
        return err;
    }
    printf("Successfully unsubscribed from signal \"bar\".\n");

    //
    // Go through the flow of creating a group, preparing it to fire, and then
    // listening for that group to fire.
    //
    err = vsi_create_signal_group(handle, 10);
    if (err)
    {
        printf("Failed to create signal group 10! Error code %d.\n", err);
        return err;
    }
    printf("Created signal group 10.\n");

    err = vsi_add_signal_to_group_by_name(handle, "gen", 10);
    if (err)
    {
        printf("Failed to add signal \"gen\" to group 10! Error code %d.\n",
               err);
        return err;
    }
    err = vsi_add_signal_to_group_by_name(handle, "ivi", 10);
    if (err)
    {
        printf("Failed to add signal \"ivi\" to group 10! Error code %d.\n",
               err);
        return err;
    }
    printf("Added signals \"gen\" and \"ivi\" to group 10.\n");

    // Initialize the memory for the array of vsi_data structures.
    my_group_data[0] = (struct vsi_data *)malloc(sizeof(struct vsi_data));
    err = vsi_name_string_to_id(handle, "gen", &domain_id, &signal_id);
    if (err)
    {
        printf("Failed to find the signal ID for the signal \"gen\"! Error code %d.\n",
               err);
        return err;
    }
    my_group_data[0]->domain_id = domain_id;
    my_group_data[0]->signal_id = signal_id;
    my_group_data[0]->data = malloc(sizeof(unsigned char));
    my_group_data[0]->len = sizeof(unsigned char);
    my_group_data[1] = (struct vsi_data *)malloc(sizeof(struct vsi_data));
    err = vsi_name_string_to_id(handle, "ivi", &domain_id, &signal_id);
    if (err)
    {
        printf("Failed to find the signal ID for the signal \"ivi\"! Error code %d.\n",
               err);
        return err;
    }
    my_group_data[1]->domain_id = domain_id;
    my_group_data[1]->signal_id = signal_id;
    my_group_data[1]->data = malloc(sizeof(unsigned char));
    my_group_data[1]->len = sizeof(unsigned char);

    //
    // As a pit stop in this example, read the current value of the group to
    // showcase the functionality.
    //
    err = vsi_get_newest_group(handle, 10, my_group_data);
    if (err)
    {
        printf("Failed to get the newest group data for group 10! Error code %d.\n",
               err);
        return err;
    }
    printf("Successfully read all signals in group 10.\n");

    err = vsi_listen(handle, &domain_id, &signal_id, &group_id, 0);
    if (err)
    {
        printf("Failed to listen for signals! Error code %d.\n", err);
        return err;
    }
    printf("Listened and got an event!\n");

    //
    // NOTE: We know that group 10 fired as it was the only group we have any
    //       signals in and we gave an infinite timeout to vsi_listen. Here is
    //       where the application would programmatically discover this.
    //
    err = vsi_get_oldest_group(handle, 10, my_group_data);
    if (err)
    {
        printf("Failed to get the oldest group data for group 10! Error code %d.\n",
               err);
        return err;
    }
    printf("Completed reading group 10.\n");

    //
    // NOTE: At this point, we could jump back to the vsi_listen call and repeat
    //       the cycle as appropriate.
    //

    // Clean up the group, since we no longer need it.
    err = vsi_delete_signal_group(handle, 10);
    if (err)
    {
        printf("Failed to delete signal group 10! Error code %d.\n", err);
        return err;
    }
    free(my_group_data[0]->data);
    free(my_group_data[0]);
    free(my_group_data[1]->data);
    free(my_group_data[1]);
    printf("Deleted group 10.\n");

    //
    // Unregister the signal we claimed ownership of.
    //
    // This is required because we registered the signal.
    //
    err = vsi_unregister_signal_by_name(handle, "foo");
    if (err)
    {
        printf("Failed to unregister the signal \"foo\"! Error code %d.\n",
               err);
        return err;
    }
    printf("Released ownership of signal \"foo\".\n");

    // Finally, we free the memory we allocated.
    vsi_destroy(&handle);
    if (handle)
    {
        printf("Failed to free memory used by VSI!\n");
        return -1;
    }
    printf("Freed the VSI memory.\n");

    // Hooray! \o/
    return 0;
}
