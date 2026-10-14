# UI command and completion channel

`nx_sim_channel` carries commands from one UI producer to simulation workers
and completions back to one UI consumer. Its fixed capacity bounds **all**
accepted commands that have not yet had their completion taken, including
queued commands, work in progress and queued completions. Each accepted
command receives a nonzero ticket and reserves one completion slot. Workers
may finish in any order; the UI matches completions by ticket.

Posting a command transfers ownership of its immutable payload only on
`NX_OK`. When the channel is full, `NX_ERROR_LIMIT` leaves the payload with
the caller, which can retry or cancel it. Taking a command transfers its
payload to the worker; the worker calls `nx_sim_message_dispose` after reading
it. A worker posts exactly one completion for each taken ticket. A completion
may contain a payload, or only a status. Its post succeeds without waiting for
UI layout because the command reserved capacity. Taking a completion transfers
its optional payload to the UI, which disposes it after processing. Empty
takes return `NX_OK` with ticket zero. Do not copy a taken message and dispose
both copies.

```c
/* UI thread: keep command ownership when the channel is full. */
uint64_t ticket = 0;
nx_result result = nx_sim_channel_post_command(channel, request,
    sizeof(*request), dispose_request, NULL, &ticket);
if (result != NX_OK) retry_or_free_request(request);

/* Worker thread: process a command outside the channel lock. */
nx_sim_message command = {0};
if (nx_sim_channel_take_command(channel, &command) == NX_OK && command.ticket) {
    uint64_t completed_ticket = command.ticket;
    nx_result work_result = process_request(command.data);
    nx_sim_message_dispose(&command);
    nx_sim_channel_post_completion(channel, completed_ticket, work_result,
                                   NULL, 0, NULL, NULL);
}

/* UI thread: consume the result by ticket. */
nx_sim_message completion = {0};
if (nx_sim_channel_take_completion(channel, &completion) == NX_OK &&
    completion.ticket) {
    apply_result(completion.ticket, completion.status, completion.data);
    nx_sim_message_dispose(&completion);
}
```

The channel allocates its command ring, completion ring, ticket table and lock
at creation. No queue operation allocates or copies payload bytes. One
metadata lock protects these structures; no callback or simulation work runs
under it. Ticket lookup currently scans at most `capacity` entries. Choose a
capacity that reflects the amount of work the UI can leave outstanding.

Stop UI and workers before destroy. If a worker has taken a command but has
not posted its completion, destroy returns `NX_ERROR_STATE` and leaves the
channel usable. Post a cancellation or failure completion for that ticket,
dispose the worker-owned command, then destroy. Destroy disposes queued
command and completion payloads; already taken payloads remain the caller's
responsibility.

Windows MSVC tests cover capacity backpressure, allocation failure, duplicate
completion rejection, reversed completion order, destroy cleanup and a
10,000-command run with two workers. Native macOS/Linux and thread-sanitizer
validation remain open.
