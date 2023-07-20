/***
 * pst.c
 * Author: Pavel Prochazka <pavel.prochazka@spongedata.cz>
 * Based on readpst.c by David Smith <dave.s@earthcorp.com>
 * Simplifies PST API and replaces readpst and lspst functionality within native code.
 *
 */

#include <regex.h>
#include <string.h>
#include "define.h"

struct file_ll {
    char *dname;
    int32_t stored_count;
    int32_t item_count;
    int32_t skip_count;
    int32_t type;
};

void create_enter_dir(struct file_ll* f, pst_item *item)
{
    pst_convert_utf8(item, &item->file_as);
    f->item_count   = 0;
    f->skip_count   = 0;
    f->type         = item->type;
    f->stored_count = (item->folder) ? item->folder->item_count : 0;
    f->dname        = strdup(item->file_as.str);
}


void close_enter_dir(struct file_ll *f)
{
    free(f->dname);
}

typedef struct item_enumerator {
    pst_item ** items;
    unsigned capacity;
    unsigned used;
    pst_file file;
    char * last_error;
    int num_error;
} item_enumerator;

item_enumerator * item_enumerator_new(unsigned capacity) {
    item_enumerator * ret = calloc(1, sizeof(item_enumerator));
    ret->capacity = (capacity > 1 ? 1024 : 1);

    ret->items = calloc(capacity, sizeof(pst_item*));
    ret->used = 0;

    return ret;
}

void item_enumerator_add(item_enumerator * self, pst_item* item) {
    unsigned ni = self->used;
    if (ni >= self->capacity) {
        // double capacity
        self->capacity *= 2;
        self->items = realloc(self->items, (self->capacity+1) * sizeof(pst_item*));
        memset(self->items + self->used, 0, sizeof(pst_item*) * (self->capacity+1-self->used)); // zero the rest
    }
    self->items[self->used++] = item;
}

#define NO_ERROR
#define ERROR_NOT_UNIQUE_MSG_STORE 1
#define ERROR_ROOT_NOT_FOUND 2
#define ERROR_OPEN 3
#define ERROR_INDEX_LOAD 4

int pst_list_impl(item_enumerator *ie, pst_item *outeritem, pst_desc_tree *d_ptr) {
    struct file_ll ff;
    pst_item *item = NULL;
    char *result = NULL;
    size_t resultlen = 0;
    size_t dateresultlen;

    DEBUG_ENT("pst_list_impl");
    memset(&ff, 0, sizeof(ff));
    create_enter_dir(&ff, outeritem); // just description not dir

    while (d_ptr) {
        if (!d_ptr->desc) {
            DEBUG_WARN(("ERROR item's desc record is NULL\n"));
            ff.skip_count++;
        }
        else {
            item = pst_parse_item(&ie->file, d_ptr, NULL);
            DEBUG_INFO(("About to process item @ %p.\n", item));

            if (item) {
                int validP = 1;
                if (item->message_store) {
                    // there should only be one message_store, and we have already done it
                    return ERROR_NOT_UNIQUE_MSG_STORE;
                }

                if (item->folder && d_ptr->child) {
                    // if this is a folder, we want to recurse into it
                    pst_convert_utf8(item, &item->file_as);
                    pst_list_impl(ie, item, d_ptr->child);

                } else if (item->contact && (item->type == PST_TYPE_CONTACT)) {
                    // CONTACT VCF like
                    if (!ff.type) ff.type = item->type;
                    // Process Contact item
                    if (ff.type != PST_TYPE_CONTACT) {
                        DEBUG_INFO(("I have a contact, but the folder isn't a contacts folder. Processing anyway\n"));
                    }

                    // escape in golang?
                    // if (item->ontact->fullname.str)
                    //     printf("\t%s", pst_rfc2426_escape(item->contact->fullname.str, &result, &resultlen));
                    // printf("\n");

                } else if (item->email && ((item->type == PST_TYPE_NOTE) || (item->type == PST_TYPE_SCHEDULE) || (item->type == PST_TYPE_REPORT))) {
                    // MAIL
                    if (!ff.type) ff.type = item->type;
                    // Process Email item
                    if ((ff.type != PST_TYPE_NOTE) && (ff.type != PST_TYPE_SCHEDULE) && (ff.type != PST_TYPE_REPORT)) {
                        DEBUG_INFO(("I have an email, but the folder isn't an email folder. Processing anyway\n"));
                    }
                } else if (item->journal && (item->type == PST_TYPE_JOURNAL)) {
                    if (!ff.type) ff.type = item->type;
                    // Process Journal item
                    if (ff.type != PST_TYPE_JOURNAL) {
                        DEBUG_INFO(("I have a journal entry, but folder isn't specified as a journal type. Processing...\n"));
                    }

                    // if (item->subject.str)
                    //     printf("Journal\t%s\n", pst_rfc2426_escape(item->subject.str, &result, &resultlen));

                } else if (item->appointment && (item->type == PST_TYPE_APPOINTMENT)) {
                    char time_buffer[30];
                    if (!ff.type) ff.type = item->type;
                    // Process Calendar Appointment item
                    DEBUG_INFO(("Processing Appointment Entry\n"));
                    if (ff.type != PST_TYPE_APPOINTMENT) {
                        DEBUG_INFO(("I have an appointment, but folder isn't specified as an appointment type. Processing...\n"));
                    }

                    // if (item->subject.str)
                    //     printf("\tSUMMARY: %s", pst_rfc2426_escape(item->subject.str, &result, &resultlen));
                    // if (item->appointment->start)
                    //     printf("\tSTART: %s", pst_rfc2445_datetime_format(item->appointment->start, sizeof(time_buffer), time_buffer));
                    // if (item->appointment->end)
                    //     printf("\tEND: %s", pst_rfc2445_datetime_format(item->appointment->end, sizeof(time_buffer), time_buffer));
                    // printf("\tALL DAY: %s", (item->appointment->all_day==1 ? "Yes" : "No"));
                    // printf("\n");

                } else {
                    ff.skip_count++;
                    DEBUG_INFO(("Unknown item type. %i. Ascii1=\"%s\"\n",
                                      item->type, item->ascii_type));
                    validP = 0;
                }

                if(validP) {
                    item_enumerator_add(ie, item);
                }
            } else {
                ff.skip_count++;
                DEBUG_INFO(("A NULL item was seen\n"));
            }
        }
        d_ptr = d_ptr->next;
    }
    close_enter_dir(&ff);
    if (result) free(result);
    DEBUG_RET();
}

item_enumerator * pst_list(const char * path) {
    item_enumerator * out = item_enumerator_new(1); // TODO: change the default pool size?

    if (pst_open(&(out->file), path, NULL)) {
        out->last_error = "Cannot open file.";
        out->num_error = ERROR_OPEN;
        goto defer;
    }

    if (pst_load_index(&(out->file))) {
        out->last_error = "Cannot load index.";
        out->num_error = ERROR_INDEX_LOAD;
        goto defer;
    }

    pst_load_extended_attributes(&(out->file));

    pst_desc_tree  *d_ptr = out->file.d_head; // first record is main record
    pst_item *item  = pst_parse_item(&out->file, d_ptr, NULL);

    if (!item || !item->message_store) {
        DEBUG_RET();
        goto defer;
    }

    char *temp  = NULL; //temporary char pointer
    // default the file_as to the same as the main filename if it doesn't exist
    if (!item->file_as.str) {
        if (!(temp = strrchr(path, '/')))
            if (!(temp = strrchr(path, '\\')))
                temp = (char*)path; // FIXME: strdup() ?
            else
                temp++; // get past the "\\"
        else
            temp++; // get past the "/"
        item->file_as.str = strdup(temp);
        item->file_as.is_utf8 = 1;
    }

    d_ptr = pst_getTopOfFolders(&out->file, item);
    if (!d_ptr) {
        out->last_error = "Root record not found.";
        out->num_error = ERROR_ROOT_NOT_FOUND;
        goto defer;
    }

    // traverse
    out->num_error = pst_list_impl(out, item, d_ptr->child);
    if (out->num_error) {
        out->last_error = "Traversing error.";
    }

defer:
    return out;
}


void item_enumerator_destroy(item_enumerator * ie) {
    pst_item ** lst = ie->items;

    while(*lst) {
        pst_freeItem(*(lst++));
    }

    pst_close(&ie->file);
    free(ie->items);
    free(ie);
}

#define PST_STORE 1
#define PST_MESSAGE 1 << 1
#define PST_FOLDER 1 << 2
#define PST_JOURNAL 1 << 3
#define PST_TYPE_APPOINTMENT 1 << 4

typedef struct pst_record
{
    /* data */
    uint8_t type;
    pst_file * pf;
    pst_item * pi;
    char * renaming;
    char * extra_mime_headers;
} pst_record;

typedef struct pst_store
{
    pst_record r;
    /* data */
} pst_store;

char * pst_store_to_string(pst_store * self, int * error) {
    *error = 0;
    return NULL; // doing nothing
}

/** .eml - convertible message (email, schedule, note, report) */
typedef struct pst_message {
    pst_record r;

    // converted data from pst_item
} pst_message;


/** *** WRITE EML PART ***/



char * pst_message_to_string(pst_message * self, int * error) {
    *error = 0;

    // c

    return NULL; // doing nothing
}


typedef struct pst_folder {
    pst_record r;

    char * path;
} pst_folder;


typedef struct pst_journal {
    pst_record r;
} pst_journal;



int main(int argc, char* const* argv) {
    if (argc < 2) {
        return 1;
    }

    const char * path = argv[1];
    item_enumerator * ie = pst_list(path);

    pst_item ** lst = ie->items;

    while(*lst) { //  pst_convert_utf8(item, &item->contact->fullname)
        //pst_convert_utf8_null(*lst, &((*lst)->contact->fullname));
        pst_item * item = *lst;

        if (item->message_store) {
            // there should only be one message_store, and we have already done it
            printf("A second message_store has been found. Sorry, this must be an error.\n");
        }
        else if (item->email && ((item->type == PST_TYPE_NOTE) || (item->type == PST_TYPE_SCHEDULE) || (item->type == PST_TYPE_REPORT))) {
            pst_convert_utf8_null(*lst, &((*lst)->subject));
            printf("\tSubject: %s\n", item->subject.str);
        }
        else if (item->folder) {
            pst_convert_utf8_null(*lst, &((*lst)->file_as));
            printf("LST: folder? %s\n", item->file_as.str);
        }
        else if (item->journal && (item->type == PST_TYPE_JOURNAL)) {

        }
        else if (item->appointment && (item->type == PST_TYPE_APPOINTMENT)) {

        }
        else if (item->message_store) {

        }
        else {
            printf(("Unknown item type %i (%s) name (%s)\n",
                        item->type, item->ascii_type, item->file_as.str));
        }

        lst++;
    }


    item_enumerator_destroy(ie);

    return 0;
}
