#ifndef EXAMPLE_AETHERI_PICKER_OWNER_H
#define EXAMPLE_AETHERI_PICKER_OWNER_H
#include "picker.h"
enum { FP_OWNER_MODAL=2, FP_OWNER_BACKDROP=3, FP_OWNER_FILE=60000,
    FP_OWNER_DIRECTORY, FP_OWNER_CLOSE, FP_OWNER_NOTE, FP_OWNER_RESULT,
    FP_OWNER_SCROLL, FP_OWNER_THUMB, FP_OWNER_NOTE_LABEL };
typedef struct fp_owner {
    nx_font_collection *fonts; /* Borrowed; the owner outlives each chooser view. */
    nx_text_field *note;
    nx_text_layout *labels[6];
    uint64_t epoch, trigger, restore, selections, revealed_focus;
    int active, modal_built, german, directories, directory, witness;
    float text_size, scroll_y, layout_width, layout_size, revealed_height;
    char label[AE_FILES_LABEL+1]; /* Copied original display label, never a path. */
} fp_owner;
nx_result fp_owner_init(fp_owner *, nx_font_collection *);
void fp_owner_destroy(fp_owner *);
/* One persistent original-model worker, fresh view epoch/edit buffer per open.
   READ rediscovers its held current folder; this does not reset navigation. */
nx_result fp_owner_open(fp_owner *, fp_picker *, fp_worker *, int directories);
/* Between frames. Close the existing modal and restore its trigger. Accepted
   selection transfers FD ownership exactly once; Cancel preserves prior result.
   Does not stop/join the original worker. Host retires the context afterward. */
nx_result fp_owner_finish(fp_owner *, fp_picker *, nx_context *, int selected);
nx_result fp_owner_build(fp_owner *, fp_picker *, nx_context *, nx_window *,
    float width, float height, fp_intent *, int *redraw, int *open_mode, int *quit);
int fp_owner_reveal(fp_owner *, const nx_context *, float height);
#endif
