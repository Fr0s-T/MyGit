#ifndef MYGIT_CHECKOUT_INDEX_H
#define MYGIT_CHECKOUT_INDEX_H

#include "checkout_entry.h"

/*
** Reads the current .mygit/index file and collects tracked checkout entries.
**
** Outputs on success:
** - current_entries: heap array of checkout_entry pointers
** - entry_count: number of collected entries
**
** Caller owns the returned array and entries.
** Use checkout_destroy_entries() to free the result.
*/
int checkout_collect_current_tracked_entries(checkout_entry ***current_entries,
    int *entry_count);

#endif
