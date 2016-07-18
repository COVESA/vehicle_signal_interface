/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


#ifndef _VSI_LIST_H_
#define _VSI_LIST_H_

#include <pthread.h>


typedef struct vsi_list_entry
{
    struct vsi_list_entry* next;
    void*                  pointer;

}   vsi_list_entry;


typedef struct vsi_list
{
    vsi_list_entry* listHead;
    vsi_list_entry* listTail;

    unsigned int    count;

    pthread_mutex_t mutex;

}   vsi_list;


int vsi_list_initialize ( vsi_list* list );

int vsi_list_insert ( vsi_list* list, void* record );

int vsi_list_remove ( vsi_list* list, void* record );

int vsi_list_remove_head ( vsi_list* list, void** record );

inline unsigned int vsi_list_get_count ( vsi_list* list )
{
    return list->count;
}


#endif // _VSI_LIST_H_
