/**
 * Various utilities
 *
 * Authors:
 *   Bob Jamison
 *
 * Copyright (C) 2013 Bob Jamison
 * 
 *  This file is part of the SdrLib library.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include "util.h"


List *listAppend(List *list, void *data)
{
    List *node = (List *)malloc(sizeof(List));
    if (!node)
        return NULL;
    node->next = NULL;
    node->data = data;
    if (list)
        {
        while (list->next)
            list = list->next;
        list->next = node;
        }
    return node;
}

List *listRemove(List *list, void *data)
{
    List *curr = list;
    List *next = NULL;
    List *prev = NULL;
    while (curr)
        {
        next = curr->next;
        if (list->data == data)
            {
            free(curr);
            if (prev)
                prev->next = next;
            else
                list = next;
            }
        prev = curr;
        curr = next;
        }
    return list;
}

int listLength(List *list)
{
    int i = 0;
    for ( ; list ; list=list->next)
        i++;
    return i;
}


void listForEach(List *list, ListFunc func)
{
    for ( ; list ; list=list->next)
        func(list->data);
}



void listDelete(List *list, ListFunc func)
{
    List *curr = list;
    List *next = NULL;
    while (curr)
        {
        next = curr->next;
        if (func)
            func(curr->data);
        free(curr);
        curr = next;
        }
}





