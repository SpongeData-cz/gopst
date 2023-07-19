/***
 * pst.c
 * Author: Pavel Prochazka <pavel.prochazka@spongedata.cz>
 * Based on readpst.c by David Smith <dave.s@earthcorp.com>
 * Simplifies PST API and replaces readpst and lspst functionality within native code.
 *
 */

#include <regex.h>
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
                temp = path;
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



// filename for the attachment
#define RTF_ATTACH_NAME "rtf-body.rtf"
// mime type for the attachment
#define RTF_ATTACH_TYPE "application/rtf"
#define RFC822          "message/rfc822"
#define C_TIME_SIZE     500


int  test_base64(char *body, size_t len)
{
    int b64 = 0;
    uint8_t *b = (uint8_t *)body;
    DEBUG_ENT("test_base64");
    while (len--) {
        if ((*b < 32) && (*b != 9) && (*b != 10)) {
            DEBUG_INFO(("found base64 byte %d\n", (int)*b));
            DEBUG_HEXDUMPC(body, strlen(body), 0x10);
            b64 = 1;
            break;
        }
        b++;
    }
    DEBUG_RET();
    return b64;
}

#define VERSION "1.0.0"



void write_schedule_part_data(FILE* f_output, pst_item* item, const char* sender, const char* method)
{
    fprintf(f_output, "BEGIN:VCALENDAR\n");
    fprintf(f_output, "VERSION:2.0\n");
    fprintf(f_output, "PRODID:LibPST v%s\n", VERSION);
    if (method) fprintf(f_output, "METHOD:%s\n", method);
    fprintf(f_output, "BEGIN:VEVENT\n");
    if (sender) {
        if (item->email->outlook_sender_name.str) {
            fprintf(f_output, "ORGANIZER;CN=\"%s\":MAILTO:%s\n", item->email->outlook_sender_name.str, sender);
        } else {
            fprintf(f_output, "ORGANIZER;CN=\"\":MAILTO:%s\n", sender);
        }
    }
    write_appointment(f_output, item);
    fprintf(f_output, "END:VCALENDAR\n");
}


void write_schedule_part(FILE* f_output, pst_item* item, const char* sender, const char* boundary)
{
    const char* method  = "REQUEST";
    const char* charset = "utf-8";
    char fname[30];
    if (!item->appointment) return;

    // inline appointment request
    fprintf(f_output, "\n--%s\n", boundary);
    fprintf(f_output, "Content-Type: %s; method=\"%s\"; charset=\"%s\"\n\n", "text/calendar", method, charset);
    write_schedule_part_data(f_output, item, sender, method);
    fprintf(f_output, "\n");

    // attachment appointment request
    snprintf(fname, sizeof(fname), "i%i.ics", rand());
    fprintf(f_output, "\n--%s\n", boundary);
    fprintf(f_output, "Content-Type: %s; charset=\"%s\"; name=\"%s\"\n", "text/calendar", "utf-8", fname);
    fprintf(f_output, "Content-Disposition: attachment; filename=\"%s\"\n\n", fname);
    write_schedule_part_data(f_output, item, sender, method);
    fprintf(f_output, "\n");
}

void write_email_body(FILE *f, char *body) {
    char *n = body;
    DEBUG_ENT("write_email_body");
    pst_fwrite(body, strlen(body), 1, f);
    DEBUG_RET();
}


void write_body_part(FILE* f_output, pst_string *body, char *mime, char *charset, char *boundary, pst_file* pst)
{
    DEBUG_ENT("write_body_part");
    removeCR(body->str);
    size_t body_len = strlen(body->str);

    if (body->is_utf8 && (strcasecmp("utf-8", charset))) {
        if (1) { //prefer_utf8) { TODO:
            charset = "utf-8";
        } else {
            // try to convert to the specified charset since the target
            // is not utf-8, and the data came from a unicode (utf16) field
            // and is now in utf-8.
            size_t rc;
            DEBUG_INFO(("Convert %s utf-8 to %s\n", mime, charset));
            pst_vbuf *newer = pst_vballoc(2);
            rc = pst_vb_utf8to8bit(newer, body->str, body_len, charset);
            if (rc == (size_t)-1) {
                // unable to convert, change the charset to utf8
                free(newer->b);
                DEBUG_INFO(("Failed to convert %s utf-8 to %s\n", mime, charset));
                charset = "utf-8";
            } else {
                // null terminate the output string
                pst_vbgrow(newer, 1);
                newer->b[newer->dlen] = '\0';
                free(body->str);
                body->str = newer->b;
                body_len = newer->dlen;
            }
            free(newer);
        }
    }
    int base64 = test_base64(body->str, body_len);
    fprintf(f_output, "\n--%s\n", boundary);
    fprintf(f_output, "Content-Type: %s; charset=\"%s\"\n", mime, charset);
    if (base64) fprintf(f_output, "Content-Transfer-Encoding: base64\n");
    fprintf(f_output, "\n");
    // Any body that uses an encoding with NULLs, e.g. UTF16, will be base64-encoded here.
    if (base64) {
        char *enc = pst_base64_encode(body->str, body_len);
        if (enc) {
            write_email_body(f_output, enc);
            fprintf(f_output, "\n");
            free(enc);
        }
    }
    else {
        write_email_body(f_output, body->str);
    }
    DEBUG_RET();
}

void removeCR (char *c) {
    // converts \r\n to \n
    char *a, *b;
    DEBUG_ENT("removeCR");
    a = b = c;
    while (*a != '\0') {
        *b = *a;
        if (*a != '\r') b++;
        a++;
    }
    *b = '\0';
    DEBUG_RET();
}

char *my_stristr(char *haystack, char *needle) {
    // my_stristr varies from strstr in that its searches are case-insensitive
    char *x=haystack, *y=needle, *z = NULL;
    if (!haystack || !needle) {
        return NULL;
    }
    while (*y != '\0' && *x != '\0') {
        if (tolower(*y) == tolower(*x)) {
            // move y on one
            y++;
            if (!z) {
                z = x; // store first position in haystack where a match is made
            }
        } else {
            y = needle; // reset y to the beginning of the needle
            z = NULL; // reset the haystack storage point
        }
        x++; // advance the search in the haystack
    }
    // If the haystack ended before our search finished, it's not a match.
    if (*y != '\0') return NULL;
    return z;
}


char* header_get_field(char *header, char *field)
{
    char *t = my_stristr(header, field);
    if (!t && (strncasecmp(header, field+1, strlen(field)-1) == 0)) t = header;
    return t;
}

void find_rfc822_headers(char** extra_mime_headers)
{
    DEBUG_ENT("find_rfc822_headers");
    char *headers = *extra_mime_headers;
    if (headers) {
        char *temp, *t;
        while ((temp = strstr(headers, "\n\n"))) {
            temp[1] = '\0';
            t = header_get_field(headers, "\nContent-Type:");
            if (t) {
                t++;
                DEBUG_INFO(("found content type header\n"));
                char *n = strchr(t, '\n');
                char *s = strstr(t, ": ");
                char *e = strchr(t, ';');
                if (!e || (e > n)) e = n;
                if (s && (s < e)) {
                    s += 2;
                    if (!strncasecmp(s, RFC822, e-s)) {
                        headers = temp+2;   // found rfc822 header
                        DEBUG_INFO(("found 822 headers\n%s\n", headers));
                        break;
                    }
                }
            }
            //DEBUG_INFO(("skipping to next block after\n%s\n", headers));
            headers = temp+2;   // skip to next chunk of headers
        }
        *extra_mime_headers = headers;
    }
    DEBUG_RET();
}


void find_html_charset(char *html, char *charset, size_t charsetlen)
{
    const int  index = 1;
    const int nmatch = index+1;
    regmatch_t match[nmatch];
    DEBUG_ENT("find_html_charset");
    regex_t     meta_charset_pattern;

    // FIXME: check compilation - do it lazy!
    regcomp(&meta_charset_pattern, "<meta[^>]*content=\"[^>]*charset=([^>\";]*)[\";]", REG_ICASE | REG_EXTENDED);

    int rc = regexec(&meta_charset_pattern, html, nmatch, match, 0);
    if (rc == 0) {
        int s = match[index].rm_so;
        int e = match[index].rm_eo;
        if (s != -1) {
            char save = html[e];
            html[e] = '\0';
                snprintf(charset, charsetlen, "%s", html+s);    // copy the html charset
            html[e] = save;
            DEBUG_INFO(("charset %s from html text\n", charset));
        }
        else {
            DEBUG_INFO(("matching %d %d %d %d\n", match[0].rm_so, match[0].rm_eo, match[1].rm_so, match[1].rm_eo));
            DEBUG_HEXDUMPC(html, strlen(html), 0x10);
        }
    }
    else {
        DEBUG_INFO(("regexec returns %d\n", rc));
    }
    DEBUG_RET();
}

#define MODE_NORMAL 0
void write_embedded_message(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pf, int save_rtf, char** extra_mime_headers)
{
    pst_index_ll *ptr;
    DEBUG_ENT("write_embedded_message");
    ptr = pst_getID(pf, attach->i_id);

    pst_desc_tree d_ptr;
    d_ptr.d_id        = 0;
    d_ptr.parent_d_id = 0;
    d_ptr.assoc_tree  = NULL;
    d_ptr.desc        = ptr;
    d_ptr.no_child    = 0;
    d_ptr.prev        = NULL;
    d_ptr.next        = NULL;
    d_ptr.parent      = NULL;
    d_ptr.child       = NULL;
    d_ptr.child_tail  = NULL;

    pst_item *item = pst_parse_item(pf, &d_ptr, attach->id2_head);
    // It appears that if the embedded message contains an appointment/
    // calendar item, pst_parse_item returns NULL due to the presence of
    // an unexpected reference type of 0x1048, which seems to represent
    // an array of GUIDs representing a CLSID. It's likely that this is
    // a reference to an internal Outlook COM class.
    //      Log the skipped item and continue on.
    if (!item) {
        DEBUG_WARN(("write_embedded_message: pst_parse_item was unable to parse the embedded message in attachment ID %llu", attach->i_id));
    } else {
        if (!item->email) {
            DEBUG_WARN(("write_embedded_message: pst_parse_item returned type %d, not an email message", item->type));
        } else {
            fprintf(f_output, "\n--%s\n", boundary);
            fprintf(f_output, "Content-Type: %s\n\n", attach->mimetype.str);
            write_normal_email(f_output, "", item, MODE_NORMAL, 0, pf, save_rtf, 1, extra_mime_headers);
        }
        pst_freeItem(item);
    }

    DEBUG_RET();
}


/**
 * write extra vcard or vcalendar categories from the extra keywords fields
 *
 * @param f_output open file pointer
 * @param item     pst item containing the keywords
 * @return         true if we write a categories line
 */
int write_extra_categories(FILE* f_output, pst_item* item)
{
    char*  result = NULL;
    size_t resultlen = 0;
    pst_item_extra_field *ef = item->extra_fields;
    const char *fmt = "CATEGORIES:%s";
    int category_started = 0;
    while (ef) {
        if (strcmp(ef->field_name, "Keywords") == 0) {
            fprintf(f_output, fmt, pst_rfc2426_escape(ef->value, &result, &resultlen));
            fmt = ", %s";
            category_started = 1;
        }
        ef = ef->next;
    }
    if (category_started) fprintf(f_output, "\n");
    if (result) free(result);
    return category_started;
}



void write_appointment(FILE* f_output, pst_item* item)
{
    char*  result = NULL;
    size_t resultlen = 0;
    char   time_buffer[30];
    pst_item_appointment* appointment = item->appointment;

    // make everything utf8
    pst_convert_utf8_null(item, &item->subject);
    pst_convert_utf8_null(item, &item->body);
    pst_convert_utf8_null(item, &appointment->location);

    fprintf(f_output, "UID:%#"PRIx64"\n", item->block_id);
    fprintf(f_output, "DTSTAMP:%s\n",                     pst_rfc2445_datetime_format_now(sizeof(time_buffer), time_buffer));
    if (item->create_date)
        fprintf(f_output, "CREATED:%s\n",                 pst_rfc2445_datetime_format(item->create_date, sizeof(time_buffer), time_buffer));
    if (item->modify_date)
        fprintf(f_output, "LAST-MOD:%s\n",                pst_rfc2445_datetime_format(item->modify_date, sizeof(time_buffer), time_buffer));
    if (item->subject.str)
        fprintf(f_output, "SUMMARY:%s\n",                 pst_rfc2426_escape(item->subject.str, &result, &resultlen));
    if (item->body.str)
        fprintf(f_output, "DESCRIPTION:%s\n",             pst_rfc2426_escape(item->body.str, &result, &resultlen));
    if (appointment && appointment->start)
        fprintf(f_output, "DTSTART;VALUE=DATE-TIME:%s\n", pst_rfc2445_datetime_format(appointment->start, sizeof(time_buffer), time_buffer));
    if (appointment && appointment->end)
        fprintf(f_output, "DTEND;VALUE=DATE-TIME:%s\n",   pst_rfc2445_datetime_format(appointment->end, sizeof(time_buffer), time_buffer));
    if (appointment && appointment->location.str)
        fprintf(f_output, "LOCATION:%s\n",                pst_rfc2426_escape(appointment->location.str, &result, &resultlen));
    if (appointment) {
        switch (appointment->showas) {
            case PST_FREEBUSY_TENTATIVE:
                fprintf(f_output, "STATUS:TENTATIVE\n");
                break;
            case PST_FREEBUSY_FREE:
                // mark as transparent and as confirmed
                fprintf(f_output, "TRANSP:TRANSPARENT\n");
            case PST_FREEBUSY_BUSY:
            case PST_FREEBUSY_OUT_OF_OFFICE:
                fprintf(f_output, "STATUS:CONFIRMED\n");
                break;
        }
        if (appointment->is_recurring) {
            const char* rules[] = {"DAILY", "WEEKLY", "MONTHLY", "YEARLY"};
            const char* days[]  = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
            pst_recurrence *rdata = pst_convert_recurrence(appointment);
            fprintf(f_output, "RRULE:FREQ=%s", rules[rdata->type]);
            if (rdata->count)       fprintf(f_output, ";COUNT=%u",      rdata->count);
            if ((rdata->interval != 1) &&
                (rdata->interval))  fprintf(f_output, ";INTERVAL=%u",   rdata->interval);
            if (rdata->dayofmonth)  fprintf(f_output, ";BYMONTHDAY=%d", rdata->dayofmonth);
            if (rdata->monthofyear) fprintf(f_output, ";BYMONTH=%d",    rdata->monthofyear);
            if (rdata->position)    fprintf(f_output, ";BYSETPOS=%d",   rdata->position);
            if (rdata->bydaymask) {
                char byday[40];
                int  empty = 1;
                int i=0;
                memset(byday, 0, sizeof(byday));
                for (i=0; i<7; i++) {
                    int bit = 1 << i;
                    if (bit & rdata->bydaymask) {
                        char temp[40];
                        snprintf(temp, sizeof(temp), "%s%s%s", byday, (empty) ? ";BYDAY=" : ";", days[i]);
                        strcpy(byday, temp);
                        empty = 0;
                    }
                }
                fprintf(f_output, "%s", byday);
            }
            fprintf(f_output, "\n");
            pst_free_recurrence(rdata);
        }
        switch (appointment->label) {
            case PST_APP_LABEL_NONE:
                if (!write_extra_categories(f_output, item)) fprintf(f_output, "CATEGORIES:NONE\n");
                break;
            case PST_APP_LABEL_IMPORTANT:
                fprintf(f_output, "CATEGORIES:IMPORTANT\n");
                break;
            case PST_APP_LABEL_BUSINESS:
                fprintf(f_output, "CATEGORIES:BUSINESS\n");
                break;
            case PST_APP_LABEL_PERSONAL:
                fprintf(f_output, "CATEGORIES:PERSONAL\n");
                break;
            case PST_APP_LABEL_VACATION:
                fprintf(f_output, "CATEGORIES:VACATION\n");
                break;
            case PST_APP_LABEL_MUST_ATTEND:
                fprintf(f_output, "CATEGORIES:MUST-ATTEND\n");
                break;
            case PST_APP_LABEL_TRAVEL_REQ:
                fprintf(f_output, "CATEGORIES:TRAVEL-REQUIRED\n");
                break;
            case PST_APP_LABEL_NEEDS_PREP:
                fprintf(f_output, "CATEGORIES:NEEDS-PREPARATION\n");
                break;
            case PST_APP_LABEL_BIRTHDAY:
                fprintf(f_output, "CATEGORIES:BIRTHDAY\n");
                break;
            case PST_APP_LABEL_ANNIVERSARY:
                fprintf(f_output, "CATEGORIES:ANNIVERSARY\n");
                break;
            case PST_APP_LABEL_PHONE_CALL:
                fprintf(f_output, "CATEGORIES:PHONE-CALL\n");
                break;
        }
        // ignore bogus alarms
        if (appointment->alarm && (appointment->alarm_minutes >= 0) && (appointment->alarm_minutes < 1440)) {
            fprintf(f_output, "BEGIN:VALARM\n");
            fprintf(f_output, "TRIGGER:-PT%dM\n", appointment->alarm_minutes);
            fprintf(f_output, "ACTION:DISPLAY\n");
            fprintf(f_output, "DESCRIPTION:Reminder\n");
            fprintf(f_output, "END:VALARM\n");
        }
    }
    fprintf(f_output, "END:VEVENT\n");
    if (result) free(result);
}



// return pointer to \n at the end of this header field,
// or NULL if this field goes to the end of the string.
char *header_end_field(char *field)
{
    char *e = strchr(field+1, '\n');
    while (e && ((e[1] == ' ') || (e[1] == '\t'))) {
        e = strchr(e+1, '\n');
    }
    return e;
}

void header_strip_field(char *header, char *field)
{
    char *t;
    while ((t = header_get_field(header, field))) {
        char *e = header_end_field(t);
        if (e) {
            if (t == header) e++;   // if *t is not \n, we don't want to keep the \n at *e either.
            while (*e != '\0') {
                *t = *e;
                t++;
                e++;
            }
            *t = '\0';
        }
        else {
            // this was the last header field, truncate the headers
            *t = '\0';
        }
    }
}



void header_get_subfield(char *field, const char *subfield, char *body_subfield, size_t size_subfield)
{
    if (!field) return;
    DEBUG_ENT("header_get_subfield");
    char search[60];
    snprintf(search, sizeof(search), " %s=", subfield);
    field++;
    char *n = header_end_field(field);
    char *s = my_stristr(field, search);
    if (n && s && (s < n)) {
        char *e, *f, save;
        s += strlen(search);    // skip over subfield=
        if (*s == '"') {
            s++;
            e = strchr(s, '"');
        }
        else {
            e = strchr(s, ';');
            f = strchr(s, '\n');
            if (e && f && (f < e)) e = f;
        }
        if (!e || (e > n)) e = n;   // use the trailing lf as terminator if nothing better
        save = *e;
        *e = '\0';
        snprintf(body_subfield, size_subfield, "%s", s);  // copy the subfield to our buffer
        *e = save;
        DEBUG_INFO(("body %s %s from headers\n", subfield, body_subfield));
    }
    DEBUG_RET();
}



int  header_match(char *header, char*field) {
    int n = strlen(field);
    if (strncasecmp(header, field, n) == 0) return 1;   // tag:{space}
    if ((field[n-1] == ' ') && (strncasecmp(header, field, n-1) == 0)) {
        char *crlftab = "\r\n\t";
        DEBUG_INFO(("Possible wrapped header = %s\n", header));
        if (strncasecmp(header+n-1, crlftab, 3) == 0) return 1; // tag:{cr}{lf}{tab}
    }
    return 0;
}


void header_has_field(char *header, char *field, int *flag)
{
    DEBUG_ENT("header_has_field");
    if (my_stristr(header, field) || (strncasecmp(header, field+1, strlen(field)-1) == 0)) {
        DEBUG_INFO(("header block has %s header\n", field+1));
        *flag = 1;
    }
    DEBUG_RET();
}


int  valid_headers(char *header)
{
    // headers are sometimes really bogus - they seem to be fragments of the
    // message body, so we only use them if they seem to be real rfc822 headers.
    // this list is composed of ones that we have seen in real pst files.
    // there are surely others. the problem is - given an arbitrary character
    // string, is it a valid (or even reasonable) set of rfc822 headers?
    if (header) {
        if (header_match(header, "Content-Type: "                 )) return 1;
        if (header_match(header, "Date: "                         )) return 1;
        if (header_match(header, "From: "                         )) return 1;
        if (header_match(header, "MIME-Version: "                 )) return 1;
        if (header_match(header, "Microsoft Mail Internet Headers")) return 1;
        if (header_match(header, "Received: "                     )) return 1;
        if (header_match(header, "Return-Path: "                  )) return 1;
        if (header_match(header, "Subject: "                      )) return 1;
        if (header_match(header, "To: "                           )) return 1;
        if (header_match(header, "X-ASG-Debug-ID: "               )) return 1;
        if (header_match(header, "X-Barracuda-URL: "              )) return 1;
        if (header_match(header, "X-x: "                          )) return 1;
        if (strlen(header) > 2) {
            DEBUG_INFO(("Ignore bogus headers = %s\n", header));
        }
        return 0;
    }
    else return 0;
}

void pst_message_extract(pst_message * self)
{
    char boundary[60];
    char altboundary[66];
    char *altboundaryp = NULL;
    char body_charset[30];
    char buffer_charset[30];
    char body_report[60];
    char sender[60];
    int  sender_known = 0;
    char *temp = NULL;
    time_t em_time;
    char *c_time;
    char *headers = NULL;
    int has_from, has_subject, has_to, has_cc, has_date, has_msgid;
    has_from = has_subject = has_to = has_cc = has_date = has_msgid = 0;

    pst_item * item = self->r.pi;
    char * extra_mime_headers = self->r.extra_mime_headers;
    FILE * f_output = fopen(self->r.renaming, "w+");

    DEBUG_ENT("write_normal_email");

    pst_convert_utf8_null(item, &item->email->header);
    headers = valid_headers(item->email->header.str) ? item->email->header.str :
              valid_headers(*extra_mime_headers)     ? *extra_mime_headers     :
              NULL;

    // setup default body character set and report type
    strncpy(body_charset, pst_default_charset(item, sizeof(buffer_charset), buffer_charset), sizeof(body_charset));
    body_charset[sizeof(body_charset)-1] = '\0';
    strncpy(body_report, "delivery-status", sizeof(body_report));
    body_report[sizeof(body_report)-1] = '\0';

    // setup default sender
    pst_convert_utf8(item, &item->email->sender_address);
    if (item->email->sender_address.str && strchr(item->email->sender_address.str, '@')) {
        temp = item->email->sender_address.str;
        sender_known = 1;
    }
    else {
        temp = "MAILER-DAEMON";
    }
    strncpy(sender, temp, sizeof(sender));
    sender[sizeof(sender)-1] = '\0';

    // convert the sent date if it exists, or set it to a fixed date
    if (item->email->sent_date) {
        em_time = pst_fileTimeToUnixTime(item->email->sent_date);
        c_time = ctime(&em_time);
        if (c_time)
            c_time[strlen(c_time)-1] = '\0'; //remove end \n
        else
            c_time = "Thu Jan 1 00:00:00 1970";
    } else
        c_time = "Thu Jan 1 00:00:00 1970";

    // create our MIME boundaries here.
    snprintf(boundary, sizeof(boundary), "--boundary-LibPST-iamunique-%i_-_-", rand());
    snprintf(altboundary, sizeof(altboundary), "alt-%s", boundary);

    // we will always look at the headers to discover some stuff
    if (headers ) {
        char *t;
        removeCR(headers);

        temp = strstr(headers, "\n\n");
        if (temp) {
            // cut off our real rfc822 headers here
            temp[1] = '\0';
            // pointer to all the embedded MIME headers.
            // we use these to find the actual rfc822 headers for embedded message/rfc822 mime parts
            // but only for the outermost message
            if (!*extra_mime_headers) *extra_mime_headers = temp+2;
            DEBUG_INFO(("Found extra mime headers\n%s\n", temp+2));
        }

        // Check if the headers have all the necessary fields
        header_has_field(headers, "\nFrom:",        &has_from);
        header_has_field(headers, "\nTo:",          &has_to);
        header_has_field(headers, "\nSubject:",     &has_subject);
        header_has_field(headers, "\nDate:",        &has_date);
        header_has_field(headers, "\nCC:",          &has_cc);
        header_has_field(headers, "\nMessage-Id:",  &has_msgid);

        // look for charset and report-type in Content-Type header
        t = header_get_field(headers, "\nContent-Type:");
        header_get_subfield(t, "charset", body_charset, sizeof(body_charset));
        header_get_subfield(t, "report-type", body_report, sizeof(body_report));

        // derive a proper sender email address
        if (!sender_known) {
            t = header_get_field(headers, "\nFrom:");
            if (t) {
                // assume address is on the first line, rather than on a continuation line
                t++;
                char *n = strchr(t, '\n');
                char *s = strchr(t, '<');
                char *e = strchr(t, '>');
                if (s && e && n && (s < e) && (e < n)) {
                char save = *e;
                *e = '\0';
                    snprintf(sender, sizeof(sender), "%s", s+1);
                *e = save;
                }
            }
        }

        // Strip out the mime headers and some others that we don't want to emit
        header_strip_field(headers, "\nMicrosoft Mail Internet Headers");
        header_strip_field(headers, "\nMIME-Version:");
        header_strip_field(headers, "\nContent-Type:");
        header_strip_field(headers, "\nContent-Transfer-Encoding:");
        header_strip_field(headers, "\nContent-class:");
        header_strip_field(headers, "\nX-MimeOLE:");
        header_strip_field(headers, "\nX-From_:");
    }

    DEBUG_INFO(("About to print Header\n"));

    if (item && item->subject.str) {
        pst_convert_utf8(item, &item->subject);
        DEBUG_INFO(("item->subject = %s\n", item->subject.str));
    }

    // MODE_SEPARATE ALWAYS IN OUR CASE

    // if (mode != MODE_SEPARATE) {
    //     // most modes need this separator line.
    //     // procmail produces this separator without the quotes around the
    //     // sender email address, but apparently some Mac email client needs
    //     // those quotes, and they don't seem to cause problems for anyone else.
    //     char *quo = (embedding) ? ">" : "";
    //     fprintf(f_output, "%sFrom \"%s\" %s\n", quo, sender, c_time);
    // }

    // print the supplied email headers
    if (headers) {
        int len = strlen(headers);
        if (len > 0) {
            fprintf(f_output, "%s", headers);
            // make sure the headers end with a \n
            if (headers[len-1] != '\n') fprintf(f_output, "\n");
            //char *h = headers;
            //while (*h) {
            //    char *e = strchr(h, '\n');
            //    int   d = 1;    // normally e points to trailing \n
            //    if (!e) {
            //        e = h + strlen(h);  // e points to trailing null
            //        d = 0;
            //    }
            //    // we could do rfc2047 encoding here if needed
            //    fprintf(f_output, "%.*s\n", (int)(e-h), h);
            //    h = e + d;
            //}
        }
    }

    // record read status
    if ((item->flags & PST_FLAG_READ) == PST_FLAG_READ) {
        fprintf(f_output, "Status: RO\n");
    }

    // create required header fields that are not already written

    if (!has_from) {
        if (item->email->outlook_sender_name.str){
            pst_rfc2047(item, &item->email->outlook_sender_name, 1);
            fprintf(f_output, "From: %s <%s>\n", item->email->outlook_sender_name.str, sender);
        } else {
            fprintf(f_output, "From: <%s>\n", sender);
        }
    }

    if (!has_subject) {
        if (item->subject.str) {
            pst_rfc2047(item, &item->subject, 0);
            fprintf(f_output, "Subject: %s\n", item->subject.str);
        } else {
            fprintf(f_output, "Subject: \n");
        }
    }

    if (!has_to && item->email->sentto_address.str) {
        pst_rfc2047(item, &item->email->sentto_address, 0);
        fprintf(f_output, "To: %s\n", item->email->sentto_address.str);
    }

    if (!has_cc && item->email->cc_address.str) {
        pst_rfc2047(item, &item->email->cc_address, 0);
        fprintf(f_output, "Cc: %s\n", item->email->cc_address.str);
    }

    if (!has_date && item->email->sent_date) {
        char c_time[C_TIME_SIZE];
        struct tm stm;
        gmtime_r(&em_time, &stm);
        strftime(c_time, C_TIME_SIZE, "%a, %d %b %Y %H:%M:%S %z", &stm);
        fprintf(f_output, "Date: %s\n", c_time);
    }

    if (!has_msgid && item->email->messageid.str) {
        pst_convert_utf8(item, &item->email->messageid);
        fprintf(f_output, "Message-Id: %s\n", item->email->messageid.str);
    }

    // add forensic headers to capture some .pst stuff that is not really
    // needed or used by mail clients
    pst_convert_utf8_null(item, &item->email->sender_address);
    if (item->email->sender_address.str && !strchr(item->email->sender_address.str, '@')
                                        && strcmp(item->email->sender_address.str, ".")
                                        && (strlen(item->email->sender_address.str) > 0)) {
        fprintf(f_output, "X-libpst-forensic-sender: %s\n", item->email->sender_address.str);
    }

    if (item->email->bcc_address.str) {
        pst_convert_utf8(item, &item->email->bcc_address);
        fprintf(f_output, "X-libpst-forensic-bcc: %s\n", item->email->bcc_address.str);
    }

    // add our own mime headers
    fprintf(f_output, "MIME-Version: 1.0\n");
    if (item->type == PST_TYPE_REPORT) {
        // multipart/report for DSN/MDN reports
        fprintf(f_output, "Content-Type: multipart/report; report-type=%s;\n\tboundary=\"%s\"\n", body_report, boundary);
    }
    else {
        fprintf(f_output, "Content-Type: multipart/mixed;\n\tboundary=\"%s\"\n", boundary);
    }
    fprintf(f_output, "\n");    // end of headers, start of body

    // now dump the body parts
    if ((item->type == PST_TYPE_REPORT) && (item->email->report_text.str)) {
        write_body_part(f_output, &item->email->report_text, "text/plain", body_charset, boundary, self->r.pf);
        fprintf(f_output, "\n");
    }

    if (item->body.str && item->email->htmlbody.str) {
        // start the nested alternative part
        fprintf(f_output, "\n--%s\n", boundary);
        fprintf(f_output, "Content-Type: multipart/alternative;\n\tboundary=\"%s\"\n", altboundary);
        altboundaryp = altboundary;
    }
    else {
        altboundaryp = boundary;
    }

    if (item->body.str) {
        write_body_part(f_output, &item->body, "text/plain", body_charset, altboundaryp, self->r.pf);
    }

    if (item->email->htmlbody.str) {
        find_html_charset(item->email->htmlbody.str, body_charset, sizeof(body_charset));
        write_body_part(f_output, &item->email->htmlbody, "text/html", body_charset, altboundaryp, self->r.pf);
    }

    if (item->body.str && item->email->htmlbody.str) {
        // end the nested alternative part
        fprintf(f_output, "\n--%s--\n", altboundary);
    }

    if (item->email->rtf_compressed.data) {
        pst_item_attach* attach = (pst_item_attach*)pst_malloc(sizeof(pst_item_attach));
        DEBUG_INFO(("Adding RTF body as attachment\n"));
        memset(attach, 0, sizeof(pst_item_attach));
        attach->next = item->attach;
        item->attach = attach;
        attach->data.data         = pst_lzfu_decompress(item->email->rtf_compressed.data, item->email->rtf_compressed.size, &attach->data.size);
        attach->filename2.str     = strdup(RTF_ATTACH_NAME);
        attach->filename2.is_utf8 = 1;
        attach->mimetype.str      = strdup(RTF_ATTACH_TYPE);
        attach->mimetype.is_utf8  = 1;
    }

    if (item->email->encrypted_body.data) {
        pst_item_attach* attach = (pst_item_attach*)pst_malloc(sizeof(pst_item_attach));
        DEBUG_INFO(("Adding encrypted text body as attachment\n"));
        memset(attach, 0, sizeof(pst_item_attach));
        attach->next = item->attach;
        item->attach = attach;
        attach->data.data = item->email->encrypted_body.data;
        attach->data.size = item->email->encrypted_body.size;
        item->email->encrypted_body.data = NULL;
    }

    if (item->email->encrypted_htmlbody.data) {
        pst_item_attach* attach = (pst_item_attach*)pst_malloc(sizeof(pst_item_attach));
        DEBUG_INFO(("Adding encrypted HTML body as attachment\n"));
        memset(attach, 0, sizeof(pst_item_attach));
        attach->next = item->attach;
        item->attach = attach;
        attach->data.data = item->email->encrypted_htmlbody.data;
        attach->data.size = item->email->encrypted_htmlbody.size;
        item->email->encrypted_htmlbody.data = NULL;
    }

    if (item->type == PST_TYPE_SCHEDULE) {
        write_schedule_part(f_output, item, sender, boundary);
    }

    // other attachments
    {
        pst_item_attach* attach;
        int attach_num = 0;
        for (attach = item->attach; attach; attach = attach->next) {
            pst_convert_utf8_null(item, &attach->filename1);
            pst_convert_utf8_null(item, &attach->filename2);
            pst_convert_utf8_null(item, &attach->mimetype);
            DEBUG_INFO(("Attempting Attachment encoding\n"));
            if (attach->method == PST_ATTACH_EMBEDDED) {
                DEBUG_INFO(("have an embedded rfc822 message attachment\n"));
                if (attach->mimetype.str) {
                    DEBUG_INFO(("which already has a mime-type of %s\n", attach->mimetype.str));
                    free(attach->mimetype.str);
                }
                attach->mimetype.str = strdup(RFC822);
                attach->mimetype.is_utf8 = 1;
                find_rfc822_headers(extra_mime_headers);
                write_embedded_message(f_output, attach, boundary, self->r.pf, 1, extra_mime_headers);
            }
            else if (attach->data.data || attach->i_id) {
                if (acceptable_ext(attach)) {
                    write_inline_attachment(f_output, attach, boundary, self->r.pf);
                }
            }
        }
    }

    fprintf(f_output, "\n--%s--\n\n", boundary);
    DEBUG_RET();
}



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
