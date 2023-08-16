
#include <regex.h>
#include <string.h>
#include "define.h"
#include "pst.h"

int main(int argc, char* const* argv) {
    if (argc < 2) {
        return 1;
    }

    const char * path = argv[1];
    pst_record_enumerator * ie = pst_list(path);
    pst_record ** lst = ie->items;
    pst_export * ppe = pst_export_new(pst_export_conf_default);
    ppe->pstfile = ie->file; // misconception

    int nth = 0;

    while(*lst) { //  pst_convert_utf8(item, &item->contact->fullname)
        //pst_convert_utf8_null(*lst, &((*lst)->contact->fullname));
        pst_record * pr = *lst;
        lst++;
        if(!pr) continue;
        printf("Known type with path %s!\n", pr->logical_path);

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
    return 0;
}