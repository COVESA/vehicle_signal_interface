/* Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** @file vsi.h
 *  @brief Function prototypes for the Vehicle Signal Interface API.
 *
 *  This file should be included by all applications intending to either produce
 *  or consume signals via the Vehicle Signal Interface. This file is
 *  responsible for exposing all available function prototypes and data types.
 *
 *  @author Matthew Vick &lt;mvick@jaguarlandrover.com&gt;
 */

#pragma once
#ifndef _VSI_H_
#define _VSI_H_

// **********
// DATA TYPES
// **********

/**
 * @brief Used to pass buffers to and return data from the API.
 *
 * Calling applications are responsible for allocating a data buffer for the API
 * to store information in and passing the pointer through the *data member.
 *
 * In addition, it is expected that applications will provide the length of the
 * data buffer in the len member variable.
 */
struct vsi_data {
    /** The domain ID for the signal tied to this data. */
    unsigned int domain_id;
    /** The signal ID for the signal tied to this data. */
    unsigned int signal_id;
    /** Pointer to the data buffer for the signal. */
    void *data;
    /** The length of the data buffer. */
    unsigned long len;
    /** The current status of the signal. */
    int status;
};

/** Application handle used to interact with the API. */
typedef void* vsi_handle;

// ***************************
// INITIALIZATION AND TEARDOWN
// ***************************

/**
 * @brief Required to initialize the API.
 *
 * This must be the first function called by applications interested in vehicle
 * signals.
 *
 * @return A handle for the API. On failure, a NULL pointer will be returned.
 */
vsi_handle vsi_initialize();

/**
 * @brief Required to tear down the API.
 *
 * Calling applications are expected to call this to cleanly tear down the API.
 *
 * @param handle Pointer to the API handle to clean up.
 * @return 0 on success or an error code on failure.
 */
int vsi_destroy(vsi_handle *handle);

// **************************************
// VSI SIGNAL REGISTRATION AND GENERATION
// **************************************

/**
 * @brief Take ownership of a signal by its name.
 *
 * For a more detailed description, please see vsi_register_signal().
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to register.
 * @return 0 on success or an error code on failure.
 */
int vsi_register_signal_by_name(vsi_handle handle, char *name);

/**
 * @brief Take ownership of a signal by its ID.
 *
 * This call is optional, but will guarantee that no other application will be
 * able to fire the signal. An error will be returned if another application has
 * already registered the signal.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_register_signal(vsi_handle handle, unsigned int domain_id,
                        unsigned int signal_id);

/**
 * @brief Release ownership of a signal by its name.
 *
 * For a more detailed description, please see vsi_unregister_signal().
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to unregister.
 * @return 0 on success or an error code on failure.
 */
int vsi_unregister_signal_by_name(vsi_handle handle, char *name);

/**
 * @brief Release ownership of a signal by its ID.
 *
 * This call is optional, but necessary if a signal has been registered by the
 * calling application to allow other applications to be able to fire the signal.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_unregister_signal(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id);

/**
 * @brief Fire a vehicle signal by its name.
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to fire.
 * @param data Pointer to the data buffer to send.
 * @param data_len Length of the data buffer.
 * @return 0 on success or an error code on failure.
 */
int vsi_fire_signal_by_name(vsi_handle handle, char *name, void *data,
                            unsigned long data_len);

/**
 * @brief Fire a vehicle signal by its ID.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @param data Pointer to the data buffer to send.
 * @param data_len Length of the data buffer.
 * @return 0 on success or an error code on failure.
 */
int vsi_fire_signal(vsi_handle handle, unsigned int domain_id,
                    unsigned int signal_id, void *data, unsigned int data_len);

// ***********************************************
// VSI SIGNAL SUBSCRIPTION AND GROUP CONFIGURATION
// ***********************************************

/**
 * @brief Subscribe to a signal to receive notifications by its name.
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to subscribe to.
 * @return 0 on success or an error code on failure.
 */
int vsi_subscribe_to_signal_by_name(vsi_handle handle, char *name);

/**
 * @brief Subscribe to a signal to receive notifications by its ID.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_subscribe_to_signal(vsi_handle handle, unsigned int domain_id,
                            unsigned int signal_id);

/**
 * @brief Unsubscribe from a signal to stop receiving notifications by its name.
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to unsubscribe from.
 * @return 0 on success or an error code on failure.
 */
int vsi_unsubscribe_from_signal_by_name(vsi_handle handle, char *name);

/**
 * @brief Unsubscribe from a signal to stop receiving notifications by its ID.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_unsubscribe_from_signal(vsi_handle handle, unsigned int domain_id,
                                unsigned int signal_id);

/**
 * @brief Creates a signal group with the specified group ID local to the
 *        calling application.
 *
 * @param handle Required handle for the API.
 * @param group_id Local group ID to create.
 * @return 0 on success or an error code on failure.
 */
int vsi_create_signal_group(vsi_handle handle, unsigned long group_id);

/**
 * @brief Deletes the specified group ID.
 *
 * @param handle Required handle for the API.
 * @param group_id Local group ID to delete.
 * @return 0 on success or an error code on failure.
 */
int vsi_delete_signal_group(vsi_handle handle, unsigned long group_id);

/**
 * @brief Adds a signal to a group by the signal's name.
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to add to the group.
 * @param group_id Affected group ID.
 * @return 0 on success or an error code on failure.
 */
int vsi_add_signal_to_group_by_name(vsi_handle handle, char *name,
                                    unsigned long group_id);

/**
 * @brief Adds a signal to a group by the signal's ID.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @param group_id Affected group ID.
 * @return 0 on success or an error code on failure.
 */
int vsi_add_signal_to_group(vsi_handle handle, unsigned int domain_id,
                            unsigned int signal_id, unsigned long group_id);

/**
 * @brief Removes a signal from a group by the signal's name.
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to remove from the group.
 * @param group_id Affected group ID.
 * @return 0 on success or an error code on failure.
 */
int vsi_remove_signal_from_group_by_name(vsi_handle handle, char *name,
                                         unsigned long group_id);

/**
 * @brief Removes a signal from a group by the signal's ID.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @param group_id Affected group ID.
 * @return 0 on success or an error code on failure.
 */
int vsi_remove_signal_from_group(vsi_handle handle, unsigned int domain_id,
                                 unsigned int signal_id,
                                 unsigned long group_id);

// ******************
// VSI SIGNAL PARSING
// ******************

/**
 * @brief Reads the latest value of the specified signal by name.
 *
 * For a more detailed description, please see vsi_get_newest_signal().
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to read.
 * @param result Used by the API to return the most recent value of a signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_get_newest_signal_by_name(vsi_handle handle, char *name,
                                  struct vsi_data *result);

/**
 * @brief Reads the latest value of the specified signal by ID.
 *
 * The calling application is responsible for providing a vsi_data structure as
 * input and for providing both a pointer to a valid data buffer and the length
 * of the buffer as information in the structure. The API will write data to the
 * buffer and overwrite the length field with the number of bytes written. If
 * the API detects an error specific to the signal, it will log an error in the
 * status field of the structure.
 *
 * This API is strictly for reading the most recent update from a signal. The
 * calling application does not need to be subscribed to a signal to read it.
 *
 * Unlike vsi_get_oldest_signal(), this call does not remove a message from the
 * queue.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @param result Used by the API to return the most recent value of a signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_get_newest_signal(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id, struct vsi_data *result);

/**
 * @brief Flushes all unread messages from the VSI message queue for the signal
 *        by name.
 *
 * For a more detailed description, please see vsi_flush_signal().
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to flush.
 * @return 0 on success or an error code on failure.
 */
int vsi_flush_signal_by_name(vsi_handle handle, char *name);

/**
 * @brief Flushes all unread messages from the VSI message queue for the signal
 *        by ID.
 *
 * If an application is no longer interested in any of the pending signals to be
 * read or simply wants to synchronize with the latest information, it may call
 * this function to flush all pending messages from the queue.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_flush_signal(vsi_handle handle, unsigned int domain_id,
                     unsigned int signal_id);

/**
 * @brief Reads the signal events from the oldest entry to the newest entry by
 *        name.
 *
 * For a more detailed description, please see vsi_get_oldest_signal().
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to read.
 * @param result Used by the API to return the value of a signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_get_oldest_signal_by_name(vsi_handle handle, char *name,
                               struct vsi_data *result);

/**
 * @brief Reads the signal events from the oldest entry to the newest entry by
 *        ID.
 *
 * The calling application is responsible for providing a vsi_data structure as
 * input and for providing both a pointer to a valid data buffer and the length
 * of the buffer as information in the structure. The API will write data to the
 * buffer and overwrite the length field with the number of bytes written. If
 * the API detects an error specific to the signal, it will log an error in the
 * status field of the structure.
 *
 * If two signals have been generated since the last read, the oldest signal will
 * be read first. Once the oldest signal has been read, the API will handle the
 * clean up such that the message cannot be read a second time by the same
 * application. Calling applications may continue to consume data by issuing
 * additional calls to vsi_get_oldest_signal until they reach the end of the
 * buffer, where an error will be returned to indicate that there is no more
 * data.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @param result Used by the API to return the value of a signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_get_oldest_signal(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id, struct vsi_data *result);

/**
 * @brief Reads the latest value of every signal in a group.
 *
 * The calling application is responsible for providing a pointer to an array of
 * vsi_data structures as input and for providing both a pointer to a valid data
 * buffer and the length of the buffer as information in each structure. The API
 * will write data to the buffer and overwrite the length field of each
 * structure with the number of bytes written. If the API detects an error
 * specific to the signal, it will log an error in the status field of the
 * structure for that signal.
 *
 * This API is strictly for reading the most recent update from each signal in
 * the group.
 *
 * Unlike vsi_get_oldest_group(), this call does not remove any messages from the
 * queues.
 *
 * @param handle Required handle for the API.
 * @param group_id Affected group ID.
 * @param result Used by the API to return the most recent values of the
 *               signals.
 * @return 0 on success or an error code on failure.
 */
int vsi_get_newest_group(vsi_handle handle, unsigned long group_id,
                         struct vsi_data **result);

/**
 * @brief Reads the signal events from the oldest entry to the newest entry for
 *        every signal in a group.
 *
 * The calling application is responsible for providing a pointer to an array of
 * vsi_data structures as input and for providing both a pointer to a valid data
 * buffer and the length of the buffer as information in each structure. The API
 * will write data to the buffer and overwrite the length field of each
 * structure with the number of bytes written. If the API detects an error
 * specific to the signal, it will log an error in the status field of the
 * structure for that signal.
 *
 * When a group is processed, the first signal since the listen call is stored. A
 * simple example with two signals in a group follows:
 *
   @verbatim
    VSI HANDLE
        |
     GROUP 1
     /     \
   SIG0   SIG1
   @endverbatim
 * 
 * VSI allows for a cache of multiple messages, but for the sake of an example
 * assume it can only hold two messages and the events happen as follows:
 *
   @verbatim
   A: The application goes into a listen state.
   B: Signal 0 fires. The information is saved in **result.
   C: Signal 0 fires.
   D: Signal 0 fires.
   E: Signal 1 fires. The information is saved in **result.
   F: The group causes a wake event.
   @endverbatim
 * 
 * Internal to the VSI cache, the information at B is lost. The listening
 * application receives the results of the signal events at B and E. Applications
 * more interested in the events of D and E should call vsi_get_newest_group()
 * followed by vsi_flush_group().
 *
 * Calling applications may continue to consume data by issuing additional calls
 * to vsi_get_oldest_group() until they reach the end of the buffer for each
 * signal, where an error will be returned to indicate that there is no more data
 * for that signal. This function will return an error code when no signals have
 * new data to consume.
 *
 * The API does not imply any correlation between the signals. In the example
 * above, calling this function a second time would return C and E and calling it
 * a third time would return D and E and an error indicating that there is no
 * more data.
 *
 * @param handle Required handle for the API.
 * @param group_id Affected group ID.
 * @param result Used by the API to return the values of the signals.
 * @return 0 on success or an error code on failure.
 */
int vsi_get_oldest_group(vsi_handle handle, unsigned long group_id,
                         struct vsi_data **result);

/**
 * @brief Flushes all unread messages from the VSI message queue for each signal
 *        in the group.
 *
 * If an application is no longer interested in any of the pending signals to be
 * read or simply wants to synchronize with the latest information, it may call
 * this function to flush all pending messages from the queues of every signal
 * in the group.
 *
 * @param handle Required handle for the API.
 * @param group_id Affected group ID.
 * @return 0 on success or an error code on failure.
 */
int vsi_flush_group(vsi_handle handle, unsigned int group_id);

/**
 * @brief Puts the application into a wait state for all subscribed signal and
 *        group events.
 *
 * The event that triggers the wake will be stored in the location pointed to by
 * the domain_id, signal_id, and group_id pointers according to the following
 * logic:
 * - If a signal fired, the ID will be stored in the domain_id and signal_id
 *   locations.
 * - If a group fired, the group number will be stored in the group_id location.
 * - If neither has fired and the optional timeout has been breached, both the
 *   signal_id and group_id locations will contain 0.
 *
 * The timeout may be omitted by setting it to zero.
 *
 * Calling applications must check the validity of the pointer to determine what
 * woke the system.
 *
 * @param handle Required handle for the API.
 * @param domain_id If set, will be the domain ID of the signal that caused the
 *                  wake event.
 * @param signal_id If set, will be the signal ID of the signal that caused the
 *                  wake event.
 * @param group_id If set, will be the group ID of the group that caused the
 *                 wake event.
 * @param timeout The (optional) timeout in milliseconds. If set to zero, the
 *                API will wait indefinitely.
 * @return 0 on success or an error code on failure.
 */
int vsi_listen(vsi_handle handle, unsigned int *domain_id,
               unsigned int *signal_id, unsigned long *group_id,
               unsigned int timeout);

// ***************
// ID MANIPULATION
// ***************

/**
 * @brief Gets the ID of a signal from the signal name.
 *
 * @param handle Required handle for the API.
 * @param name Name of the signal to get the ID for.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_name_string_to_id(vsi_handle handle, char *name,
                          unsigned int *domain_id, unsigned int *signal_id);

/**
 * @brief Gets the name of a signal from the signal ID.
 *
 * @param handle Required handle for the API.
 * @param domain_id The domain ID for the signal.
 * @param signal_id The signal ID for the signal.
 * @param name Name of the signal.
 * @return 0 on success or an error code on failure.
 */
int vsi_name_id_to_string(vsi_handle handle, unsigned int domain_id,
                          unsigned int signal_id, char *name);

#endif // _VSI_H_
