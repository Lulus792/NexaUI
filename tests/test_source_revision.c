#include <nexaui/nexaui.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const char *revision = "must be cleared";
    if (nx_get_source_revision(NULL) != NX_ERROR_ARGUMENT) return 1;
    nx_result result = nx_get_source_revision(&revision);
    if (!strcmp(argv[1], "unknown")) {
        if (result != NX_ERROR_UNSUPPORTED || revision != NULL) return 1;
    } else {
        if (result != NX_OK || !revision || strcmp(revision, argv[1])) return 1;
        const char *again = NULL;
        if (nx_get_source_revision(&again) != NX_OK || again != revision) return 1;
    }
    puts("Source revision declaration checked");
    return 0;
}
