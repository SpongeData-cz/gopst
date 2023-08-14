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
#include "pst.h"

struct file_lls {
    char *dname;
    int32_t stored_count;
    int32_t item_count;
    int32_t skip_count;
    int32_t type;
};



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

int pst_list_impl(item_enumerator *ie, pst_item *outeritem, pst_desc_tree *d_ptr) {
    pst_item *item = NULL;
    DEBUG_ENT("pst_list_impl");

    while (d_ptr) {
        if (!d_ptr->desc) {
            DEBUG_WARN(("ERROR item's desc record is NULL\n"));
        }
        else {
            item = pst_parse_item(&ie->file, d_ptr, NULL);
            DEBUG_INFO(("About to process item @ %p.\n", item));

            if (item) {
                int validP = 1;
                if (item->message_store) {
                    // there should only be one message_store, and we have already done it (TODO: ?? - keep it or ignore?)
                    return ERROR_NOT_UNIQUE_MSG_STORE;
                }

                if (item->folder && d_ptr->child) {
                    // FOLDER
                    pst_convert_utf8(item, &item->file_as);
                    pst_list_impl(ie, item, d_ptr->child);
                } else if (item->contact && (item->type == PST_TYPE_CONTACT)) {
                    // CONTACT VCF like
                } else if (item->email && ((item->type == PST_TYPE_NOTE) || (item->type == PST_TYPE_SCHEDULE) || (item->type == PST_TYPE_REPORT))) {
                    // MAIL
                } else if (item->journal && (item->type == PST_TYPE_JOURNAL)) {
                    // JOURNAL
                } else if (item->appointment && (item->type == PST_TYPE_APPOINTMENT)) {
                    // APPOINTMENT
                } else {
                    // UNKNOWN
                    validP = 0;
                }

                if(validP) {
                    item_enumerator_add(ie, item);
                }
            } else {
                DEBUG_INFO(("A NULL item was seen\n"));
            }
        }
        d_ptr = d_ptr->next;
    }

    DEBUG_RET();
    return NO_ERROR;
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


int item_enumerator_destroy(item_enumerator * ie) {
    pst_item ** lst = ie->items;

    while(*lst) {
        pst_freeItem(*(lst++));
    }

    pst_close(&ie->file);
    free(ie->items);
    free(ie);

    return 0;
}

typedef struct pst_store{
    pst_record r;
    /* data */
} pst_store;

pst_store * pst_store_new(pst_record pr) {
    pst_store * out = calloc(1, sizeof(pst_store));
    out->r = pr;
    out->r.type = PST_STORE;

    return out;
};

char * pst_store_to_string(pst_store * self, int * error) {
    *error = 0;
    return NULL; // doing nothing
}

/** .eml - convertible message (email, schedule, note, report) */
typedef struct pst_message {
    pst_record r;

    // converted data from pst_item
} pst_message;


pst_message * pst_message_new(pst_record pr) {
    pst_message * out = calloc(1, sizeof(pst_message));
    out->r = pr;
    out->r.type = PST_MESSAGE;

    return out;
};

// returns written size?
int pst_message_to_file(pst_message * self, pst_export *pe, int * error) {
    FILE * output = fopen(self->r.renaming, "w+");
    *error = NO_ERROR;

    if (!output) {
        *error = PST_MESSAGE_ERROR_FILE_ERROR;
        return 0;
    }

    if (!(pe->conf.output_type_mode & OTMODE_EMAIL)) {
        return 0;
    }
    else {
        char *extra_mime_headers = NULL;
        pst_item * item = self->r.pi;
        pst_export_conf pec = pe->conf;

        if (pe->conf.mode == MODE_SEPARATE) {
            *error = PST_MESSAGE_ERROR_UNSUPPORTED_PARAM;
            return 0;
        }
        else {
            // process this single email message, cannot fork since not separate mode
            write_normal_email(pe, output, "attachment", item, pec.mode,
                pec.mode_MH, self->r.pf, pec.save_rtf_body, 0, &extra_mime_headers);
            fclose(output);
            return 1;
        }
    }

    return 0; // doing nothing
}


typedef struct pst_folder {
    pst_record r;

    char * path;
} pst_folder;

pst_folder * pst_folder_new(pst_record pr) {
    pst_folder * out = calloc(1, sizeof(pst_folder));
    out->r = pr;
    out->r.type = PST_FOLDER;

    return out;
};

int pst_folder_to_file(pst_folder * self, pst_export *pe, int * error) {
    *error = NO_ERROR;
    return 0; // unimplemented no reason - folder isnt file
}

typedef struct pst_journal {
    pst_record r;
} pst_journal;


pst_journal * pst_journal_new(pst_record pr) {
    pst_journal * out = calloc(1, sizeof(pst_journal));
    out->r = pr;
    out->r.type = PST_JOURNAL;

    return out;
};

int pst_journal_to_file(pst_journal * self, pst_export *pe, int * error) {
    *error = NO_ERROR;

    FILE * f_output = fopen(self->r.renaming, "w+");
    if( !f_output ) {
        *error = ERROR_OPEN;
        return 0;
    }

    write_journal(f_output, self->r.pi);

    return fclose(f_output);
}

typedef struct pst_appointment {
    pst_record r;
} pst_appointment;


pst_appointment * pst_appointment_new(pst_record pr) {
    pst_appointment * out = calloc(1, sizeof(pst_appointment));
    out->r = pr;
    out->r.type = PST_APPOINTMENT;

    return out;
};

int pst_appointment_to_file(pst_appointment * self, pst_export *pe, int * error) {
    *error = NO_ERROR;

    FILE * f_output = fopen(self->r.renaming, "w+");
    if( !f_output ) {
        *error = ERROR_OPEN;
        return 0;
    }

    write_appointment(f_output, self->r.pi);

    return fclose(f_output);
}

void pst_record_destroy(pst_record * self) {
    free(self->pi);
    free(self);
}

typedef struct pst_message_store {
    pst_record r;
} pst_message_store;

pst_message_store * pst_message_store_new(pst_record pr) {
    pst_message_store * out = calloc(1, sizeof(pst_message_store));
    out->r = pr;
    out->r.type = PST_MESSAGE_STORE;

    return out;
}

int pst_message_store_to_file(pst_message_store * self, pst_export *pe, int * error) {
    *error = NO_ERROR;

    return 0;
}

pst_record * pst_record_interpret(pst_item * pi, pst_file * pf) {
    pst_item * item = pi;

    if (item->message_store) {
        return (pst_record*)pst_message_store_new((pst_record){
            .type=PST_MESSAGE_STORE,
            .pi=pi,
            .pf=pf,
            .renaming=NULL
        });
    }
    else if (item->email && ((item->type == PST_TYPE_NOTE) || (item->type == PST_TYPE_SCHEDULE) || (item->type == PST_TYPE_REPORT))) {
        return (pst_record*)pst_message_new((pst_record){
            .type=PST_MESSAGE,
            .pi=pi,
            .pf=pf,
            .renaming=NULL
        });
    }
    else if (item->folder) {
        return (pst_record*)pst_folder_new((pst_record){
            .type=PST_FOLDER,
            .pi=pi,
            .pf=pf,
            .renaming=NULL
        });
    }
    else if (item->journal && (item->type == PST_TYPE_JOURNAL)) {
        return (pst_record*)pst_journal_new((pst_record){
            .type=PST_JOURNAL,
            .pi=pi,
            .pf=pf,
            .renaming=NULL
        });
    }
    else if (item->appointment && (item->type == PST_TYPE_APPOINTMENT)) {
        return (pst_record*)pst_appointment_new((pst_record){
            .type=PST_APPOINTMENT,
            .pi=pi,
            .pf=pf,
            .renaming=NULL
        });
    }

    // Unknown type
    printf("UNKNOWN TYPE\n");
    return NULL;
}

int pst_record_to_file(pst_record * r, pst_export * e, int * error) {
    switch(r->type) {
        case PST_MESSAGE:
            return pst_message_to_file((pst_message*)r, e, error);
        break;
        case PST_APPOINTMENT:
            return pst_appointment_to_file((pst_appointment*)r, e, error);
        break;
        case PST_FOLDER:
            return pst_folder_to_file((pst_folder*)r, e, error);
        break;
        case PST_JOURNAL:
            return pst_journal_to_file((pst_journal*)r, e, error);
        break;
        case PST_MESSAGE_STORE:
            return pst_message_store_to_file((pst_message_store*)r, e, error);
        break;
    }

    *error = ERROR_UNKNOWN_RECORD;
    return -1;
}

