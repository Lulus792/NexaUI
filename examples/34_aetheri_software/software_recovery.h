#ifndef EXAMPLE_SOFTWARE_RECOVERY_H
#define EXAMPLE_SOFTWARE_RECOVERY_H
#include "software_history.h"
/* Private serialized-worker extension. No GUI query or mutating API. The caller
   supplies copied profiles/rows and brackets the entire batch with its original
   before/after context check. Full reservation binding must already be checked.
   Validates one original inventory using example33's existing owner model, then
   copies only selected kind+ID records before releasing it on this thread. */
int swh_recovery_read(struct ae_catalogue *,const sw_scope *,swh_record *,size_t);
#endif
