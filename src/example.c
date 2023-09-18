#include <regex.h>
#include <string.h>
#include "define.h"
#include "pst.h"

int main(int argc, char* const* argv) {
    if (argc < 2) {
        return 1;
    }

    const char * path = argv[1];
    pst_record_enumerator *ie = record_enumerator_new(path);
    pst_list(ie);

    pst_record ** lst = ie->items;
    pst_export * ppe = pst_export_new(pst_export_conf_default);
    ppe->pstfile = ie->file; // misconception

    int nth = 0;

    while(*lst) {
        pst_record * pr = *lst;
        lst++;
        if(!pr) continue;
        printf("Known type with path %s / %s\n", pr->logical_path, pr->name);

        char * rename = calloc(128, sizeof(char));
        snprintf(rename, 96, "output_%d.eml", nth++ );
        pr->renaming = rename;

        int error;
        uint64_t written = pst_record_to_file(pr, ppe, &error);

        free(rename);
        pr->renaming = NULL;

        printf("Written %lu, error: %d\n", written, error);
    }

    record_enumerator_destroy(ie);
    pst_export_destroy(ppe);

    return 0;
}
