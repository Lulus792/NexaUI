# Sortable table

Build and run the example with an output PPM path:

```sh
cmake --build build --config Debug --target nexaui_tables_example
build/examples/06_tables/Debug/nexaui_tables_example table.ppm
```

On Windows, add `.exe` to the executable path. Single-config generators usually
place it directly in `build/examples/06_tables/`. Add an SFNT TrueType font as
the optional second argument to render visible header and cell labels:

```sh
build/examples/06_tables/Debug/nexaui_tables_example table.ppm C:/Windows/Fonts/arial.ttf
```

The example builds a table with semantic headers, rows and cells. It clicks the
Score header in two successive frames to sort ascending and then descending.
`nx_table_sort_indices` sorts caller-owned source indices; stable row IDs and
equal-score order survive either sort. The final frame is written as a 560 ×
390 PPM image, and the console prints the final row order. Without a font file,
colored bars show the cells while accessible names still contain the data.

The table API handles header interaction and painting, but the application
owns the row data, sorting comparator and row/cell geometry. This example keeps
column widths fixed. Set `resize_id` and provide a width range in a larger
application to enable pointer and keyboard column resizing.
