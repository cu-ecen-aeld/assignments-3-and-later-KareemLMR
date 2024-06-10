/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"
/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    #ifdef __KERNEL__
    struct aesd_buffer_entry *entryptr = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
    #else
    struct aesd_buffer_entry *entryptr = (struct aesd_buffer_entry*) malloc(sizeof(struct aesd_buffer_entry));
    #endif

    uint8_t index;

    if (buffer->full && (buffer->out_offs == buffer->in_offs))
    {
        AESD_CIRCULAR_BUFFER_FOREACH(entryptr, buffer, index)
        {
            if (index < buffer->out_offs)
            {
                continue;
            }
            if (char_offset < entryptr->size)
            {
                *entry_offset_byte_rtn = char_offset;
                return entryptr;
            }
            else
            {
                char_offset -= entryptr->size;
            }
        }

        AESD_CIRCULAR_BUFFER_FOREACH(entryptr, buffer, index)
        {
            if (index >= buffer->in_offs)
            {
                break;
            }
            if (char_offset < entryptr->size)
            {
                *entry_offset_byte_rtn = char_offset;
                return entryptr;
            }
            else
            {
                char_offset -= entryptr->size;
            }
        }
    }
    else
    {
        AESD_CIRCULAR_BUFFER_FOREACH(entryptr, buffer, index)
        {
            PDEBUG("current string is: %s", entryptr->buffptr);
            if (char_offset < entryptr->size)
            {
                *entry_offset_byte_rtn = char_offset;
                return entryptr;
            }
            else
            {
                char_offset -= entryptr->size;
            }
        }
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */

    uint8_t currentInPtr = buffer->in_offs;
    uint8_t currentOutPtr = buffer->out_offs;

    buffer->entry[currentInPtr++] = *add_entry;

    if (buffer->full)
    {
        if (currentInPtr == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        {
            currentInPtr = 0;
        }
        currentOutPtr = currentInPtr;
    }
    else
    {
        if (currentInPtr == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        {
            currentInPtr = 0;
            currentOutPtr = 1;
            buffer->full = true;
        }
    }
    
    buffer->in_offs = currentInPtr;
    buffer->out_offs = currentOutPtr;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}