#ifndef PST_H
#define PST_H

#include <regex.h>

struct file_ll {
    char *name[PST_TYPE_MAX];
    char *dname;
    FILE * output[PST_TYPE_MAX];
    int32_t stored_count;
    int32_t item_count;
    int32_t skip_count;
};

typedef struct pst_export_conf {
  // global settings
  int mode;
  int mode_MH;   // a submode of MODE_SEPARATE
  int mode_EX;   // a submode of MODE_SEPARATE
  int mode_MSG;   // a submode of MODE_SEPARATE
  int mode_thunder;   // a submode of MODE_RECURSE
  int output_mode;
  int contact_mode; // not used within the code
  int deleted_mode; // not used within the code
  int output_type_mode;    // Default to all. not used within the code
  int contact_mode_specified; // not used within the code
  int overwrite;
  int prefer_utf8;
  int save_rtf_body; // unused
  int file_name_len;     // enough room for MODE_SPEARATE file name
  char* acceptable_extensions;
} pst_export_conf;

typedef struct pst_export {
  // global settings
  pst_export_conf conf;
  pst_file    pstfile;
  regex_t     meta_charset_pattern;
} pst_export;

#define OUTPUT_TEMPLATE "%s.%s"
#define OUTPUT_KMAIL_DIR_TEMPLATE ".%s.directory"
#define KMAIL_INDEX "../.%s.index"
#define SEP_MAIL_FILE_TEMPLATE "%i%s"

// max size of the c_time char*. It will store the date of the email
#define C_TIME_SIZE 500



// Normal mode just creates mbox format files in the current directory. Each file is named
// the same as the folder's name that it represents
#define MODE_NORMAL 0

// KMail mode creates a directory structure suitable for being used directly
// by the KMail application
#define MODE_KMAIL 1

// recurse mode creates a directory structure like the PST file. Each directory
// contains only one file which stores the emails in mboxrd format.
#define MODE_RECURSE 2

// separate mode creates the same directory structure as recurse. The emails are stored in
// separate files, numbering from 1 upward. Attachments belonging to the emails are
// saved as email_no-filename (e.g. 1-samplefile.doc or 1-Attachment2.zip)
#define MODE_SEPARATE 3


// Output Normal just prints the standard information about what is going on
#define OUTPUT_NORMAL 0

// Output Quiet is provided so that only errors are printed
#define OUTPUT_QUIET 1

// default mime-type for attachments that have a null mime-type
#define MIME_TYPE_DEFAULT "application/octet-stream"
#define RFC822            "message/rfc822"

// output mode for contacts
#define CMODE_VCARD 0
#define CMODE_LIST  1

// output mode for deleted items
#define DMODE_EXCLUDE 0
#define DMODE_INCLUDE 1

// Output type mode flags
#define OTMODE_EMAIL        1
#define OTMODE_APPOINTMENT  2
#define OTMODE_JOURNAL      4
#define OTMODE_CONTACT      8

// output settings for RTF bodies
// filename for the attachment
#define RTF_ATTACH_NAME "rtf-body.rtf"
// mime type for the attachment
#define RTF_ATTACH_TYPE "application/rtf"


void      write_email_body(pst_export * self, FILE *f, char *body);
int       acceptable_ext(pst_export * self, pst_item_attach* attach);
void      write_body_part(pst_export * self, FILE* f_output, pst_string *body, char *mime, char *charset, char *boundary, pst_file* pst);
void      write_normal_email(pst_export * self, FILE* f_output, char f_name[], pst_item* item, int mode, int mode_MH, pst_file* pst, int save_rtf, int embedding, char** extra_mime_headers);
void      write_embedded_message(pst_export * self, FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pf, int save_rtf, char** extra_mime_headers);
void      find_html_charset(pst_export * self, char *html, char *charset, size_t charsetlen);
void      removeCR(char *c);
void      usage();
void      version();
void      mk_kmail_dir(char* fname);
int       close_kmail_dir();
void      mk_recurse_dir(char* dir);
int       close_recurse_dir();
void      mk_separate_dir(char *dir);
int       close_separate_dir();
void      mk_separate_file(struct file_ll *f, int32_t t, char *extension, int openit);
void      close_separate_file(struct file_ll *f);
char*     my_stristr(char *haystack, char *needle);
void      check_filename(char *fname);
void      write_separate_attachment(char f_name[], pst_item_attach* attach, int attach_num, pst_file* pst);
void      write_inline_attachment(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pst);
int       valid_headers(char *header);
void      header_has_field(char *header, char *field, int *flag);
void      header_get_subfield(char *field, const char *subfield, char *body_subfield, size_t size_subfield);
char*     header_get_field(char *header, char *field);
char*     header_end_field(char *field);
void      header_strip_field(char *header, char *field);
int       test_base64(char *body, size_t len);
void      find_rfc822_headers(char** extra_mime_headers);
void      write_schedule_part_data(FILE* f_output, pst_item* item, const char* sender, const char* method);
void      write_schedule_part(FILE* f_output, pst_item* item, const char* sender, const char* boundary);
void      write_vcard(FILE* f_output, pst_item *item, pst_item_contact* contact, char comment[]);
int       write_extra_categories(FILE* f_output, pst_item* item);
void      write_journal(FILE* f_output, pst_item* item);
void      write_appointment(FILE* f_output, pst_item *item);
char*     quote_string(char *inp);

static const pst_export_conf pst_export_conf_default;
pst_export * pst_export_new(pst_export_conf conf);

typedef struct pst_record
{
    /* data */
    uint8_t type;
    pst_file * pf;
    pst_item * pi;
    char * logical_path;
    char * name;
    char * renaming;
    char * extra_mime_headers;
} pst_record;

typedef struct pst_record_enumerator {
    pst_record ** items;
    unsigned capacity;
    unsigned used;
    pst_file file;
    char * last_error;
    int num_error;
} pst_record_enumerator;

#define PST_STORE 1
#define PST_MESSAGE 1 << 1
#define PST_FOLDER 1 << 2
#define PST_JOURNAL 1 << 3
#define PST_APPOINTMENT 1 << 4
#define PST_MESSAGE_STORE 1 << 5

#define PST_MESSAGE_ERROR_FILE_ERROR 1
#define PST_MESSAGE_ERROR_UNSUPPORTED_PARAM 2

#define NO_ERROR 0
#define ERROR_NOT_UNIQUE_MSG_STORE 1
#define ERROR_ROOT_NOT_FOUND 2
#define ERROR_OPEN 3
#define ERROR_INDEX_LOAD 4
#define ERROR_UNKNOWN_RECORD 5

extern pst_export * pst_export_new(pst_export_conf conf);
void pst_export_destroy(pst_export * self);
pst_record_enumerator * pst_list(const char * path);
pst_record * pst_record_interpret(pst_item * pi, pst_file * pf);
void pst_record_destroy(pst_record * self);
int pst_record_to_file(pst_record * r, pst_export * e, int * error);
int item_enumerator_destroy(pst_record_enumerator * ie);
int record_enumerator_destroy(pst_record_enumerator * ie);

static const pst_export_conf pst_export_conf_default = {
  .mode = MODE_NORMAL,
  .mode_MH = 0,
  .mode_EX = 0,
  .mode_MSG = 0,
  .mode_thunder = 0,
  .output_mode = OUTPUT_NORMAL,
  .contact_mode = CMODE_VCARD,
  .deleted_mode = DMODE_EXCLUDE,
  .output_type_mode = 0xff, // all
  .contact_mode_specified = 0,
  .overwrite = 0,
  .prefer_utf8 = 1,
  .save_rtf_body = 0,
  .file_name_len = 10,
  .acceptable_extensions = NULL
};

#endif