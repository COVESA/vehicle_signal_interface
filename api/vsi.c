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
#include "vsi_types.h"

// ******************
// INTERNAL FUNCTIONS
// ******************

union vsi_signal_id vsi_name_string_to_id_internal(char *name);

//
// Helper macro to get a signal ID from a specified name. If the signal could
// not be found, an error will be returned. Rather than duplicate this logic
// across every function, define a macro for simplicity.
//
// This macro will trigger a return with an error code if one is returned from
// vsi_name_string_to_id. An error is detected by checking if the result of the
// call is non-zero.
//
#define VSI_GET_VALID_ID(name)                     \
    do                                             \
    {                                              \
        if (!(name))                               \
            return -EINVAL;                        \
        id = vsi_name_string_to_id_internal(name); \
        if ((int)id.parts.signal_id)               \
            return (int)id.parts.signal_id;        \
    } while (0)

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

    return (vsi_handle)context;
}

int vsi_destroy(vsi_handle *handle)
{
    free(handle);
    handle = NULL;

    return 0;
}

// **************************************
// VSI SIGNAL REGISTRATION AND GENERATION
// **************************************

int vsi_register_signal_by_name(vsi_handle handle, char *name)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID(name);

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

    VSI_GET_VALID_ID(name);

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

    VSI_GET_VALID_ID(name);

    return vsi_fire_signal(handle, id.parts.domain_id, id.parts.signal_id, data,
                           data_len);
}

int vsi_fire_signal(vsi_handle handle, unsigned int domain_id,
                    unsigned int signal_id, void *data, unsigned int data_len)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && data && data_len);

    context = (struct vsi_context *)handle;

    return 0;
}

// ***********************************************
// VSI SIGNAL SUBSCRIPTION AND GROUP CONFIGURATION
// ***********************************************

int vsi_subscribe_to_signal_by_name(vsi_handle handle, char *name)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID(name);

    return vsi_subscribe_to_signal(handle, id.parts.domain_id,
                                   id.parts.signal_id);
}

int vsi_subscribe_to_signal(vsi_handle handle, unsigned int domain_id,
                            unsigned int signal_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle);

    context = (struct vsi_context *)handle;

    return 0;
}

int vsi_unsubscribe_from_signal_by_name(vsi_handle handle, char *name)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID(name);

    return vsi_unsubscribe_from_signal(handle, id.parts.domain_id,
                                       id.parts.signal_id);
}

int vsi_unsubscribe_from_signal(vsi_handle handle, unsigned int domain_id,
                                unsigned int signal_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle);

    context = (struct vsi_context *)handle;

    return 0;
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

    VSI_GET_VALID_ID(name);

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

    VSI_GET_VALID_ID(name);

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

    VSI_GET_VALID_ID(name);

    return vsi_get_newest_signal(handle, id.parts.domain_id, id.parts.signal_id,
                                 result);
}

int vsi_get_newest_signal(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id, struct vsi_data *result)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && result && result->data);

    *(unsigned char *)result->data = (unsigned char)42;
    result->len = 1;
    result->status = 0;

    return 0;
}

int vsi_get_oldest_signal_by_name(vsi_handle handle, char *name,
                                  struct vsi_data *result)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID(name);

    return vsi_get_oldest_signal(handle, id.parts.domain_id, id.parts.signal_id,
                                 result);
}

int vsi_get_oldest_signal(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id, struct vsi_data *result)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && result && result->data);

    *(unsigned char *)result->data = (unsigned char)100;
    result->len = 1;
    result->status = 0;

    // Pretend this was the only message in the buffer.
    return -ENODATA;
}

int vsi_flush_signal_by_name(vsi_handle handle, char *name)
{
    union vsi_signal_id id;

    VSI_GET_VALID_ID(name);

    return vsi_flush_signal(handle, id.parts.domain_id, id.parts.signal_id);
}

int vsi_flush_signal(vsi_handle handle, unsigned int domain_id,
                     unsigned int signal_id)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle);

    context = (struct vsi_context *)handle;

    return 0;
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

    // Pretend signal 1 fired.
    *domain_id = 0;
    *signal_id = 1;

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

    *domain_id = 1;
    *signal_id = 2;

    return 0;
}

int vsi_name_id_to_string(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id, char *name)
{
    struct vsi_context *context;

    CHECK_AND_RETURN_IF_ERROR(handle && name);

    // Give some dummy signal name for now.
    strcpy(name, "org.genivi.sensor\0");

    return 0;
}

// ******************
// INTERNAL FUNCTIONS
// ******************

union vsi_signal_id vsi_name_string_to_id_internal(char *name)
{
    union vsi_signal_id id;

    id.full_id = 1;

    return id;
}
