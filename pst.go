package gopst

/*
#cgo LDFLAGS: ./src/libgopst.a ./src/libpst.a -lz
#cgo CFLAGS: -I./src/ -I./src/deps/libpst/ -I./src/deps/libpst/src/

#include <define.h>

#include "pst.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"math/bits"
	"strings"
	"unsafe"
)

const POINTER_SIZE = bits.UintSize / 8

const (
	/*
		Normal mode just creates mbox format files in the current directory. Each file is named
		the same as the folder's name that it represents.
	*/
	MODE_NORMAL = iota
	/*
		KMail mode creates a directory structure suitable for being used directly
		by the KMail application.
	*/
	MODE_KMAIL
	/*
		Recurse mode creates a directory structure like the PST file. Each directory
		contains only one file which stores the emails in mboxrd format.
	*/
	MODE_RECURSE
	/*
		Separate mode creates the same directory structure as recurse. The emails are stored in
		separate files, numbering from 1 upward. Attachments belonging to the emails are
		saved as email_no-filename (e.g. 1-samplefile.doc or 1-Attachment2.zip).
	*/
	MODE_SEPARATE
)

const (
	/*
		Output Normal just prints the standard information about what is going on.
	*/
	OUTPUT_NORMAL = iota
	/*
		Output Quiet is provided so that only errors are printed.
	*/
	OUTPUT_QUIET
)

/*
Output mode for contacts.
*/
const (
	CMODE_VCARD = iota
	CMODE_LIST
)

/*
Output mode for deleted items.
*/
const (
	DMODE_EXCLUDE = iota
	DMODE_INCLUDE
)

/*
Output type mode flags.
*/
const (
	OTMODE_EMAIL = 1 << iota
	OTMODE_APPOINTMENT
	OTMODE_JOURNAL
	OTMODE_CONTACT
)

const (
	NO_ERROR = iota
	ERROR_NOT_UNIQUE_MSG_STORE
	ERROR_ROOT_NOT_FOUND
	ERROR_OPEN
	ERROR_INDEX_LOAD
	ERROR_UNKNOWN_RECORD
)

type Export struct {
	ExportC *C.struct_pst_export
}
type ExportConf struct {
	Mode                 int
	ModeMH               int // a submode of MODE_SEPARATE
	ModeEX               int // a submode of MODE_SEPARATE
	ModeMSG              int // a submode of MODE_SEPARATE
	ModeThunder          int // a submode of MODE_RECURSE
	OutputMode           int
	ContactMode          int // not used within the code
	DeletedMode          int // not used within the code
	OutputTypeMode       int // Default to all. not used within the code
	ContactModeSpecified int // not used within the code
	Overwrite            int
	PreferUtf8           int
	SaveRtfBody          int // unused
	FileNameLen          int // enough room for MODE_SPEARATE file name
	AcceptableExtensions string
}

func NewExport(conf ExportConf) *Export {

	exportC := C.pst_export_new(goExportConfToC(conf))
	if exportC == nil {
		return nil
	}

	return &Export{ExportC: exportC}
}

func goExportConfToC(goExportConf ExportConf) C.struct_pst_export_conf {
	cExport := C.struct_pst_export_conf{
		mode:                   C.int(goExportConf.Mode),
		mode_MH:                C.int(goExportConf.ModeMH),
		mode_EX:                C.int(goExportConf.ModeMH),
		mode_MSG:               C.int(goExportConf.ModeMSG),
		mode_thunder:           C.int(goExportConf.ModeThunder),
		output_mode:            C.int(goExportConf.OutputMode),
		contact_mode:           C.int(goExportConf.ContactMode),
		deleted_mode:           C.int(goExportConf.DeletedMode),
		output_type_mode:       C.int(goExportConf.OutputTypeMode),
		contact_mode_specified: C.int(goExportConf.ContactModeSpecified),
		overwrite:              C.int(goExportConf.Overwrite),
		prefer_utf8:            C.int(goExportConf.PreferUtf8),
		save_rtf_body:          C.int(goExportConf.SaveRtfBody),
		file_name_len:          C.int(goExportConf.FileNameLen),
	}

	if goExportConf.AcceptableExtensions == "" {
		cExport.acceptable_extensions = nil
	} else {
		cExport.acceptable_extensions = C.CString(goExportConf.AcceptableExtensions)
	}

	return cExport
}

func ExportConfDefault() ExportConf {
	return ExportConf{
		Mode:                 MODE_NORMAL,
		ModeMH:               0,
		ModeEX:               0,
		ModeMSG:              0,
		ModeThunder:          0,
		OutputMode:           OUTPUT_NORMAL,
		ContactMode:          CMODE_VCARD,
		DeletedMode:          DMODE_EXCLUDE,
		OutputTypeMode:       0xff,
		ContactModeSpecified: 0,
		Overwrite:            0,
		PreferUtf8:           1,
		SaveRtfBody:          0,
		FileNameLen:          10,
		AcceptableExtensions: "",
	}
}

type Record struct {
	TypeOfRecord uint8
	// Dont needed?
	// PFile            *C.struct_pst_file
	// PItem            *C.struct_pst_item
	LogicalPath      string
	Name             string
	Renaming         string
	ExtraMimeHeaders string
	Record           *C.struct_pst_record
}
type RecordEnumerator struct {
	Items            []*Record
	Capacity         uint
	Used             uint
	File             *C.struct_pst_file
	LastError        string
	NumError         int
	recordEnumerator *C.struct_pst_record_enumerator
}

func List(path string) *RecordEnumerator {

	ego := new(RecordEnumerator)

	enum := C.pst_list(C.CString(path))

	ego.LastError = C.GoString((*enum).last_error)
	ego.NumError = int((*enum).num_error)
	if ego.NumError != C.NO_ERROR {
		return ego
	}

	ego.recordEnumerator = enum

	ego.Items = make([]*Record, 0)

	for elem := (*enum).items; *elem != nil; elem = (**C.struct_pst_record)(unsafe.Add(unsafe.Pointer(elem), POINTER_SIZE)) {
		ego.Items = append(ego.Items, &Record{
			TypeOfRecord: uint8((*elem)._type),
			// PFile:            (*elem).pf,
			// PItem:            (*elem).pi,
			LogicalPath:      C.GoString((*elem).logical_path),
			Name:             C.GoString((*elem).name),
			Renaming:         C.GoString((*elem).renaming),
			ExtraMimeHeaders: C.GoString((*elem).extra_mime_headers),
			Record:           (*elem),
		})
	}

	ego.Capacity = uint((*enum).capacity)
	ego.Used = uint((*enum).used)
	ego.File = &(*enum).file

	return ego
}

func (ego *Record) SetRecordRenaming(renaming string) {
	ego.Record.renaming = C.CString(renaming)
	ego.Renaming = strings.Clone(renaming)
}

func (ego *Record) RecordToFile(export *Export, err int) (int, int) {
	errC := C.int(0)
	written := C.pst_record_to_file(ego.Record, export.ExportC, &errC)
	return int(written), int(errC)
}

func (ego *Record) DestroyRecord() error {
	if ego == nil || ego.Record == nil {
		return fmt.Errorf("Record has been already destroyed.")
	}
	ego.Record = nil
	return nil
}

func (ego *RecordEnumerator) DestroyRecordEnumerator() error {

	if ego == nil || ego.recordEnumerator == nil {
		return fmt.Errorf("RecordEnumerator has been already destroyed.")
	}

	C.record_enumerator_destroy(ego.recordEnumerator)

	for i, r := range ego.Items {
		if err := r.DestroyRecord(); err != nil {
			return err
		}
		ego.Items[i] = nil
	}
	ego.Items = nil
	ego.File = nil
	ego.recordEnumerator = nil

	return nil
}

func (ego *Export) DestroyExport() error {
	if ego == nil || ego.ExportC == nil {
		return fmt.Errorf("Export has been already destroyed.")
	}
	C.pst_export_destroy(ego.ExportC)
	ego.ExportC = nil
	return nil
}
