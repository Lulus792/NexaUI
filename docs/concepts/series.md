# Time-series data views and decimation

`nx_series_view` borrows an immutable sequence of `{x, y}` points. Its sample
callback lets the application expose arrays of structures, separate coordinate
arrays, or a simulation snapshot without copying the full series into NexaUI.
Keep the storage stable for the entire `nx_series_decimate` call. For a
simulation-owned series, acquire an `nx_snapshot_view` first and release it
after decimation and any other reads of its points.

Points must have finite coordinates and globally nondecreasing `x`; repeated
timestamps are allowed. The callback must return a point for every index less
than `count`. The caller is responsible for this ordering invariant. NexaUI
checks every point it reads for finite coordinates and checks order while it
scans the visible range. It does not validate offscreen points every frame.
Use a one-time ingestion check if the producer can deliver unsorted data.
Gaps represented by NaN are not supported in this first contract: split a
gapped signal into separate views.

```c
typedef struct series_storage {
    const nx_series_point *points;
} series_storage;

static nx_result read_sample(void *user, size_t index,
                             nx_series_point *out) {
    const series_storage *storage = (const series_storage *)user;
    *out = storage->points[index];
    return NX_OK;
}

series_storage storage = {points};
nx_series_view view = {point_count, &storage, read_sample};
nx_series_envelope columns[800];
size_t used = 0;
nx_result result = nx_series_decimate(&view, visible_min_x, visible_max_x,
                                       800, columns, 800, &used);
if (result == NX_OK) {
    for (size_t i = 0; i < used; ++i) {
        /* Draw first, minimum, maximum and last in source-index order. */
        draw_envelope(&columns[i]);
    }
}
```

The viewport endpoints are inclusive. `columns` is the number of horizontal
pixel buckets. The caller supplies room for at least that many envelopes when
the source is nonempty; it can reuse the buffer across frames. Each occupied
column holds its first and last point plus its lowest and highest `y`, all
with original source indices. Ties retain the earlier point. Some columns can
be empty; returned envelopes are sorted by column. To draw a chronological
polyline, sort the four marks within each envelope by index and omit duplicate
indices. The x coordinate remains the point's actual x, not the center of its
pixel bucket. This preserves short spikes while limiting drawing work to at
most four marks per occupied column. The decimator does not create draw
commands or allocate memory.

Two binary searches find the inclusive visible range, then one scan groups its
points. For `N` total and `V` visible points, time is `O(log N + V)` and
library-owned extra space is `O(1)` beyond the caller's `O(columns)` output.
Dense full-range views still scan every point; the algorithm does not maintain
a persistent multiresolution index. In a moving-window plot with frequent
full-range redraws, a future indexed data source could reduce that scan.

On error, `*out_count` is zero and output contents are unspecified. An
insufficient buffer returns `NX_ERROR_LIMIT` before any sample call. Callback
errors propagate. `NX_ERROR_ARGUMENT` covers invalid ranges, nonfinite points
and visible ordering violations. `nx_series_decimate` does not retain pointers
or use another thread.
