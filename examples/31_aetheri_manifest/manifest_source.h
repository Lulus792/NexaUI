#ifndef EXAMPLE_AETHERI_MANIFEST_SOURCE_H
#define EXAMPLE_AETHERI_MANIFEST_SOURCE_H
#include "hierarchy_view.h"

/* Read-only original Aetheri manifest projection. The expected digest identifies
   bytes selected by the caller; this adapter confers no publisher trust,
   filesystem/payload permission, installation or action authority.
   No UI/context access. Output is owned, not borrowed from decoder input.
   Outputs stay unchanged on failure. A view must retain one exact expected
   artifact; changing it requires retiring the view/context and using a new epoch.
   Within that artifact, the original canonical sorted unique path ordinal is a
   stable key. No row ID has meaning across different artifact/view lifetimes. */
nx_result amh_project(const unsigned char *bytes, size_t size,
    const unsigned char expected[32], ah_key key, ah_response *output);
/* Opens only this caller-selected regular manifest, never a declared path.
   Nonblocking/no-follow open avoids FIFO and symlink waits. Reads are bounded by
   the original schema; no timeout guarantee is made for an arbitrary filesystem.
   Source and expected bytes must remain immutable during this call. */
nx_result amh_read(const char *source, const unsigned char expected[32],
    ah_key key, ah_response *output);
int amh_digest(const char *hex, unsigned char output[32]);
#endif
