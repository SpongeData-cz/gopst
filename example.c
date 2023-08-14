
#include <regex.h>
#include <string.h>
#include "define.h"
#include "pst.h"

int main(int argc, char* const* argv) {
    if (argc < 2) {
        return 1;
    }

    const char * path = argv[1];
    item_enumerator * ie = pst_list(path);
    pst_item ** lst = ie->items;
    pst_export * ppe = pst_export_new(pst_export_conf_default);
    ppe->pstfile = ie->file;

    int nth = 0;

    while(*lst) { //  pst_convert_utf8(item, &item->contact->fullname)
        //pst_convert_utf8_null(*lst, &((*lst)->contact->fullname));
        pst_item * item = *lst;
        lst++;

        pst_record * pr = pst_record_interpret(item, &(ie->file));
        if(!pr) continue;
        printf("Known type!\n");

        char * rename = calloc(128, sizeof(char));
        snprintf(rename, 96, "output_%d.eml", nth++ );
        pr->renaming = rename;

        int error;
        uint64_t written = pst_record_to_file(pr, ppe, &error);

        free(rename);
        pr->renaming = NULL;

        printf("Written %lu, error: %d\n", written, error);
    }

    item_enumerator_destroy(ie);
    return 0;
}
