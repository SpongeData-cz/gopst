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
	"math/bits"
	"unsafe"
)

const POINTER_SIZE = bits.UintSize / 8

type PstRecord struct {
	TypeOfRecord     uint8
	PFile            *C.struct_pst_file
	PItem            *C.struct_pst_item
	LogicalPath      string
	Name             string
	Renaming         string
	ExtraMimeHeaders string
}

type PstExport struct {
	pstExport *C.struct_pst_export
}

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

type pstExportConf struct {
	mode                 int
	modeMH               int // a submode of MODE_SEPARATE
	modeEX               int // a submode of MODE_SEPARATE
	modeMSG              int // a submode of MODE_SEPARATE
	modeThunder          int // a submode of MODE_RECURSE
	outputMode           int
	contactMode          int // not used within the code
	deletedMode          int // not used within the code
	outputTypeMode       int // Default to all. not used within the code
	contactModeSpecified int // not used within the code
	overwrite            int
	preferUtf8           int
	saveRtfBody          int    // unused
	fileNameLen          int    // enough room for MODE_SPEARATE file name
	acceptableExtensions string //TODO: Need to pass NULL to C functions.
}

type PstRecordEnumerator struct {
	Items     []PstRecord
	Capacity  uint
	Used      uint
	File      *C.struct_pst_file
	LastError string
	NumError  int
}

func PstList(path string) PstRecordEnumerator {

	out := PstRecordEnumerator{}

	enum := C.pst_list(C.CString(path))

	out.LastError = C.GoString((*enum).last_error)
	out.NumError = int((*enum).num_error)
	if out.NumError != C.NO_ERROR {
		return out
	}

	out.Items = make([]PstRecord, 0)

	for elem := (*enum).items; *elem != nil; elem = (**C.struct_pst_record)(unsafe.Add(unsafe.Pointer(elem), POINTER_SIZE)) {
		out.Items = append(out.Items, PstRecord{
			TypeOfRecord:     uint8((*elem)._type),
			PFile:            (*elem).pf,
			PItem:            (*elem).pi,
			LogicalPath:      C.GoString((*elem).logical_path),
			Name:             C.GoString((*elem).name),
			Renaming:         C.GoString((*elem).renaming),
			ExtraMimeHeaders: C.GoString((*elem).extra_mime_headers),
		})
	}

	out.Capacity = uint((*enum).capacity)
	out.Used = uint((*enum).used)
	out.File = &(*enum).file

	return out
}

func NewPstExport(conf pstExportConf) PstExport {
	return PstExport{
		pstExport: C.pst_export_new(goExportConfToC(conf)),
	}
}

func PstExporConfDefault() pstExportConf {
	return pstExportConf{
		mode:                 MODE_NORMAL,
		modeMH:               0,
		modeEX:               0,
		modeMSG:              0,
		modeThunder:          0,
		outputMode:           OUTPUT_NORMAL,
		contactMode:          CMODE_VCARD,
		deletedMode:          DMODE_EXCLUDE,
		outputTypeMode:       0xff,
		contactModeSpecified: 0,
		overwrite:            0,
		preferUtf8:           1,
		saveRtfBody:          0,
		fileNameLen:          10,
		acceptableExtensions: ""}
}

func goExportConfToC(goExportConf pstExportConf) C.struct_pst_export_conf {
	return C.struct_pst_export_conf{
		mode:                   C.int(goExportConf.mode),
		mode_MH:                C.int(goExportConf.modeMH),
		mode_EX:                C.int(goExportConf.modeMH),
		mode_MSG:               C.int(goExportConf.modeMSG),
		mode_thunder:           C.int(goExportConf.modeThunder),
		output_mode:            C.int(goExportConf.outputMode),
		contact_mode:           C.int(goExportConf.contactMode),
		deleted_mode:           C.int(goExportConf.deletedMode),
		output_type_mode:       C.int(goExportConf.outputTypeMode),
		contact_mode_specified: C.int(goExportConf.contactModeSpecified),
		overwrite:              C.int(goExportConf.overwrite),
		prefer_utf8:            C.int(goExportConf.preferUtf8),
		save_rtf_body:          C.int(goExportConf.saveRtfBody),
		file_name_len:          C.int(goExportConf.fileNameLen),
		acceptable_extensions:  C.CString(goExportConf.acceptableExtensions),
	}

}
