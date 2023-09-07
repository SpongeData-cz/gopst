package gopst

/*
#cgo LDFLAGS: ./src/libgopst.a /usr/local/lib/libpst.a -lz
#cgo CFLAGS: -I./src/ -I/usr/local/include/libpst-4/libpst/

#include <libpst.h>
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

/*
Size of pointer in bytes.
*/
const POINTER_SIZE = bits.UintSize / 8

/*
Errors.
*/
const (
	NO_ERROR = iota
	ERROR_NOT_UNIQUE_MSG_STORE
	ERROR_ROOT_NOT_FOUND
	ERROR_OPEN
	ERROR_INDEX_LOAD
	ERROR_UNKNOWN_RECORD
)

// EXPORT
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

type Export struct {
	pstExport *C.struct_pst_export
}
type ExportConf struct {
	Mode                 int
	ModeMH               int // A submode of MODE_SEPARATE
	ModeEX               int // A submode of MODE_SEPARATE
	ModeMSG              int // A submode of MODE_SEPARATE
	ModeThunder          int // A submode of MODE_RECURSE
	OutputMode           int
	ContactMode          int // Not used within the code
	DeletedMode          int // Not used within the code
	OutputTypeMode       int // Default to all. Not used within the code
	ContactModeSpecified int // Not used within the code
	Overwrite            int
	PreferUtf8           int
	SaveRtfBody          int // Unused
	FileNameLen          int // Enough room for MODE_SPEARATE file name
	AcceptableExtensions string
}

/*
Creates a new instance of the Export structure.
Has to be deallocated with Destroy method after use.

Parameters:
  - conf - Export Configuration.

Returns:
  - Newly created Export structure.
*/
func NewExport(conf ExportConf) *Export {

	exportC := C.pst_export_new(*goExportConfToC(conf))
	if exportC == nil {
		return nil
	}

	return &Export{pstExport: exportC}
}

/*
Creates an equivalent C exportConf structure to the <goExportConf> parameter.

Parameters:
  - goExportConf - Go version of Export configuration.

Returns:
  - Created C exportConf structure.
*/
func goExportConfToC(goExportConf ExportConf) *C.struct_pst_export_conf {
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

	return &cExport
}

/*
Default parameters of ExportConf.

Returns:
  - Newly created ExportConf structure with default parameters.
*/
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
		OutputTypeMode:       0xff, // all
		ContactModeSpecified: 0,
		Overwrite:            0,
		PreferUtf8:           1,
		SaveRtfBody:          0,
		FileNameLen:          10,
		AcceptableExtensions: "",
	}
}

/*
Destroys Export.

Returns:
  - Error, if Export has been already destroyed.
*/
func (ego *Export) Destroy() error {
	if ego == nil || ego.pstExport == nil {
		return fmt.Errorf("Export has been already destroyed.")
	}
	C.free(unsafe.Pointer(ego.pstExport.conf.acceptable_extensions)) // ?
	C.pst_export_destroy(ego.pstExport)
	ego.pstExport = nil
	return nil
}

// RECORD
type Record struct {
	TypeOfRecord     uint8
	LogicalPath      string
	Name             string
	Renaming         string
	ExtraMimeHeaders string
	Err              int
	record           *C.struct_pst_record
}

/*
Sets renaming for the Record.

Parameters:
  - renaming - Path with a new name.
*/
func (ego *Record) SetRecordRenaming(renaming string) {
	ego.record.renaming = C.CString(renaming)
	ego.Renaming = strings.Clone(renaming)
}

/*
Writes individual records to a file.

Parameters:
  - export - Pst export.

Return:
  - 1, if successfully written, otherwise 0.
*/
func (ego *Record) RecordToFile(export *Export) int {
	err := C.int(0)
	written := C.pst_record_to_file(ego.record, export.pstExport, &err)
	ego.Err = int(err)
	return int(written)
}

/*
Destroys individual Record.

Returns:
  - Error, if Record has been already destroyed.
*/
func (ego *Record) Destroy() error {
	if ego == nil || ego.record == nil {
		return fmt.Errorf("Record has been already destroyed.")
	}
	C.free(unsafe.Pointer(ego.record.renaming))
	ego.record.renaming = nil
	ego.record = nil
	return nil
}

/*
Destroys a slice of Records.

Parameters:
  - records - slice of Records to be destroyed.

Returns:
  - error if any of the Record has already been destroyed.
*/
func DestroyList(records []*Record) error {
	for i, record := range records {
		err := record.Destroy()
		records[i] = nil
		if err != nil {
			return err
		}
	}
	return nil
}

// PST
type Pst struct {
	Capacity  uint
	Used      uint
	file      *C.struct_pst_file
	LastError string
	NumError  int
	pst       *C.struct_pst_record_enumerator
}

/*
Creates a new Pst.
Has to be deallocated with Destroy method after use.

Parameters:
  - path - path to the existing Pst.

Returns:
  - Pointer to a new instance of Pst.
*/
func NewPst(path string) *Pst {

	ego := new(Pst)

	cPath := C.CString(path)
	pst := C.pst_list(cPath)
	C.free(unsafe.Pointer(cPath))

	ego.pst = pst

	ego.LastError = C.GoString((*pst).last_error)
	ego.NumError = int((*pst).num_error)
	if ego.NumError != NO_ERROR {
		return ego
	}

	ego.Capacity = uint((*pst).capacity)
	ego.Used = uint((*pst).used)
	ego.file = &(*pst).file

	return ego
}

/*
Lists content of an Pst in form of arrays.

Records must be destroyed by DestroyList() call explicitly.
Alternatively, it is possible to destroy individual Records using the Destroy() function.

Returns:
  - Slice of Records.
*/
func (ego *Pst) List() []*Record {
	records := make([]*Record, 0)

	for elem := ego.pst.items; *elem != nil; elem = (**C.struct_pst_record)(unsafe.Add(unsafe.Pointer(elem), POINTER_SIZE)) {
		records = append(records, &Record{
			TypeOfRecord:     uint8((*elem)._type),
			LogicalPath:      C.GoString((*elem).logical_path),
			Name:             C.GoString((*elem).name),
			Renaming:         C.GoString((*elem).renaming),
			ExtraMimeHeaders: C.GoString((*elem).extra_mime_headers),
			record:           (*elem),
			Err:              NO_ERROR,
		})
	}
	return records
}

/*
Destroys Pst.

Returns:
  - Error, if Pst has been already destroyed.
*/
func (ego *Pst) Destroy() error {

	if ego == nil || ego.pst == nil {
		return fmt.Errorf("Pst has been already destroyed.")
	}

	if ego.NumError != NO_ERROR {
		ego.pst.last_error = nil
		C.free(unsafe.Pointer(ego.pst.items))
		C.free(unsafe.Pointer(ego.pst))
	} else {
		C.record_enumerator_destroy(ego.pst)
	}

	ego.file = nil
	ego.pst = nil

	return nil
}
