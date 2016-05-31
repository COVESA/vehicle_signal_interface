/* Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vsi.h"
#include "vsi_list.h"
#include "vsi_types.h"
#include "vsi_core_api.h"

// **********************
// INTERNAL FUNCTIONALITY
// **********************

struct vsi_signal_wait_ctl {
    sem_t *parent_sem;
    sem_t *wake_ctl;
    struct vsi_signal *wake_signal;
};

struct vsi_signal_wait_data {
    struct vsi_signal_wait_ctl *ctl;
    struct vsi_signal *curr_signal;
};

void vsi_name_string_to_id_internal ( char* name, union vsi_signal_id* id );
int vsi_wait_for_all_signals(struct vsi_context *context,
                             unsigned int *domain_id, unsigned int *signal_id,
                             unsigned long *group_id, unsigned int timeout);
void *vsi_wait_for_signal(void *signal_ptr);

//
// Helper macro to get a signal ID from a specified name. If the signal could
// not be found, an error will be returned. Rather than duplicate this logic
// across every function, define a macro for simplicity.
//
// This macro will trigger a return with an error code if one is returned from
// vsi_name_string_to_id. An error is detected by checking if the result of the
// call is non-zero.
//
#define VSI_GET_VALID_ID( name, id )              \
{                                                 \
    if ( !(name) )                                \
    {                                             \
        return -EINVAL;                           \
    }                                             \
    vsi_name_string_to_id_internal ( name, &id ); \
}

//
// Helper macro strictly for checking if input arguments are valid. This should
// be used for brevity. The expected usage is to pass in the conditions expected
// to be true and this macro will error if any of them are not.
//
#define CHECK_AND_RETURN_IF_ERROR(input) \
    do                                   \
    {                                    \
        if (!(input))                    \
            return -EINVAL;              \
    } while (0)

// ***************************
// INITIALIZATION AND TEARDOWN
// ***************************

vsi_handle vsi_initialize()
{
    struct vsi_context *context;

    context = malloc(sizeof(struct vsi_context));
    if (!context)
        return NULL;

    // Set the lists for signals and groups to totally empty.
    context->signal_head = NULL;
    context->group_head = NULL;
    context->num_signals = 0;

    //
    //  Go initialize the core data store system.
    //
    vsi_core_handle coreHandle = vsi_core_open();
    if ( coreHandle == 0 )
    {
        printf ( "ERROR: Unable to initialize the VSI core data store!\n" );
        exit ( 255 );
    }
    context->coreHandle = coreHandle;

    return (vsi_handle)context;
}

int vsi_destroy(vsi_handle *handle)
{
    struct vsi_context *context = (struct vsi_context *)(*handle);

    // Clean up the list of signals.
    if (context)
    {
        struct vsi_signal_list_entry *curr, *prev;

        curr = context->signal_head;

        // Iterate through the list, freeing entries along the way.
        while (curr)
        {
            prev = curr;
            curr = curr->next;
            free(prev);
        }
    }
    //
    //  Close the VSI core data store.
    //
    vsi_core_close ( context->coreHandle );

    // Finally, free the handle.
    free(*handle);
    *handle = NULL;

    return 0;
}

// **************************************
// VSI SIGNAL REGISTRATION AND GENERATION
// **************************************

int vsi_register_signal_by_name(vsi_handle handle, char *name)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_register_signal(handle, id.parts.domain_id, id.parts.signal_id);
}

int vsi_register_signal(vsi_handle handle, unsigned int domain_id,
                        unsigned int signal_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle);

    context = (struct vsi_context *)handle;

    return 0;
}

int vsi_unregister_signal_by_name(vsi_handle handle, char *name)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_unregister_signal(handle, id.parts.domain_id,
                                 id.parts.signal_id);
}

int vsi_unregister_signal(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle);

    context = (struct vsi_context *)handle;

    return 0;
}

int vsi_fire_signal_by_name(vsi_handle handle, char *name, void *data,
                            unsigned long data_len)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_fire_signal(handle, id.parts.domain_id, id.parts.signal_id, data,
                           data_len);
}

int vsi_fire_signal(vsi_handle handle, unsigned int domain_id,
                    unsigned int signal_id, void *data, unsigned int data_len)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR ( handle && data && data_len );

    context = (struct vsi_context*)handle;

    vsi_core_insert ( context->coreHandle, domain_id, signal_id, data_len, data );

    return 0;
}

// ***********************************************
// VSI SIGNAL SUBSCRIPTION AND GROUP CONFIGURATION
// ***********************************************

int vsi_subscribe_to_signal_by_name(vsi_handle handle, char *name)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_subscribe_to_signal(handle, id.parts.domain_id,
                                   id.parts.signal_id);
}

int vsi_subscribe_to_signal(vsi_handle handle, unsigned int domain_id,
                            unsigned int signal_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle);

    context = (struct vsi_context *)handle;

    return vsi_list_insert(context, domain_id, signal_id);
}

int vsi_unsubscribe_from_signal_by_name(vsi_handle handle, char *name)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_unsubscribe_from_signal(handle, id.parts.domain_id,
                                       id.parts.signal_id);
}

int vsi_unsubscribe_from_signal(vsi_handle handle, unsigned int domain_id,
                                unsigned int signal_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle);

    context = (struct vsi_context *)handle;

    return vsi_list_remove(handle, domain_id, signal_id);
}

int vsi_create_signal_group(vsi_handle handle, unsigned long group_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && group_id);

    context = (struct vsi_context *)handle;

    return 0;
}

int vsi_delete_signal_group(vsi_handle handle, unsigned long group_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && group_id);

    context = (struct vsi_context *)handle;

    return 0;
}

int vsi_add_signal_to_group_by_name(vsi_handle handle, char *name,
                                    unsigned long group_id)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_add_signal_to_group(handle, id.parts.domain_id,
                                   id.parts.signal_id, group_id);
}

int vsi_add_signal_to_group(vsi_handle handle, unsigned int domain_id,
                            unsigned int signal_id, unsigned long group_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && group_id);

    context = (struct vsi_context *)handle;

    return 0;
}

int vsi_remove_signal_from_group_by_name(vsi_handle handle, char *name,
                                         unsigned long group_id)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_remove_signal_from_group(handle, id.parts.domain_id,
                                        id.parts.signal_id, group_id);
}

int vsi_remove_signal_from_group(vsi_handle handle, unsigned int domain_id,
                                 unsigned int signal_id, unsigned long group_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && group_id);

    context = (struct vsi_context *)handle;

    return 0;
}

// ******************
// VSI SIGNAL PARSING
// ******************

int vsi_get_newest_signal_by_name(vsi_handle handle, char *name,
                                  struct vsi_data *result)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_get_newest_signal(handle, id.parts.domain_id, id.parts.signal_id,
                                 result);
}

int vsi_get_newest_signal(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id, struct vsi_data *result)
{
    struct vsi_context *context = handle;

    CHECK_AND_RETURN_IF_ERROR(handle && result && result->data);

    result->len = 1;

    result->status = vsi_core_fetch_newest ( context->coreHandle, domain_id,
                                             signal_id, &result->len,
                                             &result->data );
    return result->status;
}

int vsi_get_oldest_signal_by_name(vsi_handle handle, char *name,
                                  struct vsi_data *result)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_get_oldest_signal(handle, id.parts.domain_id, id.parts.signal_id,
                                 result);
}

int vsi_get_oldest_signal(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id, struct vsi_data *result)
{
    struct vsi_context *context = handle;

    CHECK_AND_RETURN_IF_ERROR(handle && result && result->data);

    result->len = 1;

    result->status = vsi_core_fetch ( context->coreHandle, domain_id, signal_id,
                                      &result->len, &result->data );
    return result->status;
}

int vsi_flush_signal_by_name(vsi_handle handle, char *name)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID ( name, id );

    return vsi_flush_signal(handle, id.parts.domain_id, id.parts.signal_id);
}

int vsi_flush_signal(vsi_handle handle, unsigned int domain_id,
                     unsigned int signal_id)
{
    struct vsi_context *context = handle;

    CHECK_AND_RETURN_IF_ERROR(handle);

    return vsi_core_flush_signal ( context->coreHandle, domain_id, signal_id );
}

int vsi_get_newest_group(vsi_handle handle, unsigned long group_id,
                         struct vsi_data **result)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && result);
    //
    // TODO: Error checking for pointers within the group. This cannot be done
    //       in a sane manner before the group logic has been implemented. The
    //       below check is the best we can do until then.
    //
    CHECK_AND_RETURN_IF_ERROR(result[0]->data);

    // Only place a result in the first entry for now.
    *(unsigned char *)result[0]->data = (unsigned char)42;
    result[0]->len = 1;
    result[0]->status = 0;

    return 0;
}

int vsi_get_oldest_group(vsi_handle handle, unsigned long group_id,
                         struct vsi_data **result)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && result);
    //
    // TODO: Error checking for pointers within the group. This cannot be done
    //       in a sane manner before the group logic has been implemented. The
    //       below check is the best we can do until then.
    //
    CHECK_AND_RETURN_IF_ERROR(result[0]->data);

    // Only place a result in the first entry for now.
    *(unsigned char *)result[0]->data = (unsigned char)42;
    result[0]->len = 1;
    result[0]->status = 0;

    return 0;
}

int vsi_flush_group(vsi_handle handle, unsigned int group_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && group_id);

    context = (struct vsi_context *)handle;

    return 0;
}

int vsi_listen(vsi_handle handle, unsigned int *domain_id,
               unsigned int *signal_id, unsigned long *group_id,
               unsigned int timeout)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && domain_id && signal_id && group_id);

    context = (struct vsi_context *)handle;

    // TODO: Add group support.

    // We must have at least one signal to listen for.
    if (!context->signal_head && !context->group_head)
    {
        //
        // Because we know only signals are supported, put dummy values in if
        // there is no signal list, since it must be for groups.
        //
        // TODO: Remove this once group support has been added.
        //
        if (!context->signal_head)
        {
            *domain_id = 0;
            *signal_id = 1;
            *group_id = 0;
            return 0;
        }

        return -EINVAL;
    }

    //
    // TODO: The code below will trigger the listening state of the application.
    //       It should not be enabled until the VSI core API has been updated to
    //       account for the needs of this API.
    //
#if 0
    //
    // Hand the list of signals down to the VSI core API.
    //
    // TODO: Define how to do this for groups.
    //
    vsi_core_fetch_signals(context->coreHandle, context->signal_head);

    // Wait for a signal or group to fire.
    vsi_wait_for_all_signals(context, domain_id, signal_id, group_id, timeout);
#else
    // Return dummy values for now.
    *domain_id = 0;
    *signal_id = 1;
#endif

    return 0;
}

// ***************
// ID MANIPULATION
// ***************

int vsi_name_string_to_id(vsi_handle handle, char *name,
                          unsigned int *domain_id, unsigned int *signal_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && name && domain_id && signal_id);

    // Give some dummy signal values in a crazy inefficient manner for now.
    *domain_id = 0;
    if (!strcmp(name, "foo"))
        *signal_id = 1;
    else if (!strcmp(name, "bar"))
        *signal_id = 2;
    else if (!strcmp(name, "baz"))
        *signal_id = 3;
    else if (!strcmp(name, "gen"))
        *signal_id = 4;
    else if (!strcmp(name, "ivi"))
        *signal_id = 5;

    return 0;
}

int vsi_name_id_to_string(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id, char *name)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && name);

    // Only support domain 0 for dummy data.
    if (!domain_id)
        return -EINVAL;

    // Give some dummy signal name for now.
    switch (signal_id)
    {
        case 1:
            strcpy(name, "foo");
            break;
        case 2:
            strcpy(name, "bar");
            break;
        case 3:
            strcpy(name, "baz");
            break;
        case 4:
            strcpy(name, "gen");
            break;
        case 5:
            strcpy(name, "ivi");
            break;
        default:
            strcpy(name, "error");
            break;
    }

    return 0;
}

// ******************
// INTERNAL FUNCTIONS
// ******************

void vsi_name_string_to_id_internal ( char* name,
                                      union vsi_signal_id* id )
{
    //
    // TODO: Perform the signal ID lookup from VSS. These values are only good
    //       for the sample application.
    //
    id->parts.domain_id = 0;
    if (!strcmp(name, "foo"))
        id->parts.signal_id = 1;
    else if (!strcmp(name, "bar"))
        id->parts.signal_id = 2;
    else if (!strcmp(name, "baz"))
        id->parts.signal_id = 3;
    else if (!strcmp(name, "gen"))
        id->parts.signal_id = 4;
    else if (!strcmp(name, "ivi"))
        id->parts.signal_id = 5;

    id->parts.domain_id = 1;
}

int vsi_wait_for_all_signals(struct vsi_context *context,
                             unsigned int *domain_id, unsigned int *signal_id,
                             unsigned long *group_id, unsigned int timeout)
{
    pthread_t *signal_threads, *curr_signal;
    unsigned long i = 0;
    struct vsi_signal_list_entry *curr;
    struct vsi_signal_wait_data *wait_arg;
    struct vsi_signal_wait_ctl wait_arg_ctl;
    struct vsi_signal wake_info;
    sem_t control_sem, wakeup_sem;
    int err;

    // Initialize the semaphores used to wake the main thread.
    if (!!sem_init(&control_sem, 0, 0))
        return errno;
    if (!!sem_init(&wakeup_sem, 0, 1))
    {
        sem_destroy(&control_sem);
        return errno;
    }

    // TODO: Add group support.

    // TODO: Add timeout support.

    //
    // Allocate the structures we need for threading.
    //
    // TODO: Maintain the threads with the signal structures themselves.
    //
    signal_threads = malloc(sizeof(pthread_t) * (context->num_signals + 1));
    if (!signal_threads)
    {
        if (!!sem_destroy(&wakeup_sem) || !!sem_destroy(&control_sem))
            return errno;

        return -ENOMEM;
    }

    //
    // Allocate the structures we need for the individual threads.
    //
    // TODO: Maintain the threads with the signal structures themselves.
    //
    wait_arg = malloc(sizeof(struct vsi_signal_wait_data) *
                      (context->num_signals + 1));
    if (!wait_arg)
    {
        free(signal_threads);

        if (!!sem_destroy(&wakeup_sem) || !!sem_destroy(&control_sem))
            return errno;

        return -ENOMEM;
    }

    // Initialize the control structure.
    wait_arg_ctl.parent_sem = &control_sem;
    wait_arg_ctl.wake_ctl = &wakeup_sem;
    wait_arg_ctl.wake_signal = &wake_info;

    // Spawn off child threads to wait on individual signals.
    for (curr = context->signal_head; curr; curr = curr->next)
    {
        wait_arg[i].ctl = &wait_arg_ctl;
        wait_arg[i].curr_signal = &curr->signal;
        err = pthread_create(&signal_threads[i++], NULL, vsi_wait_for_signal,
                             &wait_arg[i]);
        if (err)
            return err;
    }

    // Wait for something to happen.
    if (!!sem_wait(&control_sem))
    {
        free(signal_threads);

        if (!!sem_destroy(&wakeup_sem) || !!sem_destroy(&control_sem))
            return errno;

        return errno;
    }

    // Close out all threads.
    for (i = 0; i < context->num_signals; i++)
    {
        pthread_cancel(signal_threads[i]);
        pthread_join(signal_threads[i], NULL);
    }

    //
    // Get the information about what signal fired to indicate to the calling
    // application.
    //
    *domain_id = wake_info.signal_id.parts.domain_id;
    *signal_id = wake_info.signal_id.parts.signal_id;
    *group_id = wake_info.group_id;

    //
    // Clean up like good corporate citizens. Do not bother returning an error
    // if the free fails.
    //
    free(wait_arg);
    free(signal_threads);

    return 0;
}

void *vsi_wait_for_signal(void *signal_ptr)
{
    struct vsi_signal_wait_data *signal =
                                      (struct vsi_signal_wait_data *)signal_ptr;
    int err;

    // Wait for the signal to fire.
    err = sem_wait(&signal->curr_signal->__sem);
    if (err)
        printf("Error %d waiting for the semaphore!\n", errno);

    // Try to gain control of the data structure to indicate the wake data.
    err = sem_trywait(signal->ctl->wake_ctl);
    if (err)
        return NULL;

    // Got the wake data semaphore! Copy this signal as the one that woke.
    memcpy(signal->ctl->wake_signal, signal->curr_signal,
           sizeof(struct vsi_signal));

    // Wake up the main thread.
    sem_post(signal->ctl->parent_sem);
}
