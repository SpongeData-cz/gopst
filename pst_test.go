package gopst_test

import (
	"fmt"
	"os"
	"testing"

	. "github.com/SpongeData-cz/gopst"
)

func TestPst(t *testing.T) {

	path := "./src/"

	// printEnum := func(enum *RecordEnumerator) {
	// 	println("\n--PRINTING ENUM--")
	// 	for i, e := range enum.Items {
	// 		fmt.Printf("Item number %d: %+v\n", i, e)
	// 		fmt.Printf("	TypeOfRecord: %d\n	LogicalPath: %s\n	Name: %s\n	Renaming: %s\n	ExtraMImeHeaders: %s\n\n",
	// 			e.TypeOfRecord,
	// 			// e.PFile,
	// 			// e.PItem,
	// 			e.LogicalPath,
	// 			e.Name,
	// 			e.Renaming,
	// 			e.ExtraMimeHeaders,
	// 		)
	// 	}
	// 	fmt.Printf(
	// 		"Capacity: %d\nUsed: %d\nLastError: %s\nNumError: %d\n",
	// 		enum.Capacity,
	// 		enum.Used,
	// 		enum.LastError,
	// 		enum.NumError,
	// 	)
	// 	println("\n")
	// }

	setup := func() error {
		if err := os.Mkdir("./src/out", os.ModePerm); err != nil {
			return err
		}
		return nil
	}

	read := func() (int, error) {
		files, err := os.ReadDir("./src/out")
		if err != nil {
			return 0, err
		}
		return len(files), nil
	}

	clean := func() error {
		if err := os.RemoveAll("./src/out"); err != nil {
			return err
		}
		return nil
	}

	t.Run("example", func(t *testing.T) {

		enum := NewRecordEnumerator(path + "sample.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		ret := NewExport(ExportConfDefault())
		if ret == nil {
			t.Error("Should return valid Pst Export")
		}

		if errS := setup(); errS != nil {
			t.Error(errS.Error())
		}

		records := enum.List()

		for i, curr := range records {
			newName := fmt.Sprintf("out/output_%d.eml", i)
			curr.SetRecordRenaming(path + newName)

			written, errNum := curr.RecordToFile(ret)
			if errNum != NO_ERROR {
				t.Error("Something went wrong in RecordToFile function.")
			}
			fmt.Printf("WRITTEN: %d, ERROR: %d\n", written, errNum)

		}

		if err := enum.DestroyRecordEnumerator(); err != nil {
			t.Error(err.Error())
		}

		if err := DestroyList(records); err != nil {
			t.Error(err.Error())
		}

		if err := ret.DestroyExport(); err != nil {
			t.Error(err.Error())
		}

		if errC := clean(); errC != nil {
			t.Error(errC.Error())
		}
	})

	t.Run("init", func(t *testing.T) {

		// Export init
		ret := NewExport(ExportConfDefault())
		if ret == nil {
			t.Error("Should return valid Pst Export")
		}
		if err := ret.DestroyExport(); err != nil {
			t.Error(err.Error())
		}

		// RecordEnumerator init
		enum := NewRecordEnumerator(path + "sample.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}
		if err := enum.DestroyRecordEnumerator(); err != nil {
			t.Error(err.Error())
		}

		// Non-existing path RecordEnumerator init
		// TODO: Printing error on stderr, make it shhhh?
		enumNon := NewRecordEnumerator("./im/not/existing.pst")
		if enumNon.NumError != ERROR_OPEN {
			t.Error("Should throw ERROR_OPEN error.")
		}
		if err := enumNon.DestroyRecordEnumerator(); err != nil {
			t.Error(err.Error())
		}

	})

	t.Run("list", func(t *testing.T) {

		enum := NewRecordEnumerator(path + "sample.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		records := enum.List()

		if len(records) != 2 {
			t.Errorf("Expected 2 records, got: %d\n", len(records))
		}

		if err := enum.DestroyRecordEnumerator(); err != nil {
			t.Error(err.Error())
		}

		if err := DestroyList(records); err != nil {
			t.Error(err.Error())
		}

	})

	t.Run("renaming", func(t *testing.T) {

		enum := NewRecordEnumerator(path + "sample.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		records := enum.List()
		for i, curr := range records {
			newName := fmt.Sprintf("out/output_%d.eml", i)
			curr.SetRecordRenaming(path + newName)
		}

		for i, curr := range records {
			name := fmt.Sprintf("%sout/output_%d.eml", path, i)
			if curr.Renaming != name {
				t.Errorf("Expected renaming: \"%s\", got: \"%s\"", name, curr.Renaming)
			}
		}

		if err := enum.DestroyRecordEnumerator(); err != nil {
			t.Error(err.Error())
		}

		if err := DestroyList(records); err != nil {
			t.Error(err.Error())
		}
	})

	t.Run("extraction", func(t *testing.T) {

		enum := NewRecordEnumerator(path + "backup.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		conf := ExportConfDefault()
		conf.AcceptableExtensions = "YOY"

		ret := NewExport(conf)
		if ret == nil {
			t.Error("Should return valid Pst Export")
		}

		if errS := setup(); errS != nil {
			t.Error(errS.Error())
		}

		records := enum.List()
		if len(records) != 370 {
			t.Errorf("Expected 370 records, got %d\n", len(records))
		}

		for i, curr := range records {
			newName := fmt.Sprintf("out/output_%d.eml", i)
			curr.SetRecordRenaming(path + newName)

			_, errNum := curr.RecordToFile(ret)
			if errNum != NO_ERROR {
				t.Error("Something went wrong in RecordToFile function.")
			}
		}

		if err := enum.DestroyRecordEnumerator(); err != nil {
			t.Error(err.Error())
		}

		if err := enum.DestroyRecordEnumerator(); err == nil {
			t.Error("Should return an error.")
		}

		if err := DestroyList(records); err != nil {
			t.Error(err.Error())
		}

		if err := DestroyList(records); err == nil {
			t.Error("Should return en error.")
		}

		if err := ret.DestroyExport(); err != nil {
			t.Error(err.Error())
		}

		if err := ret.DestroyExport(); err == nil {
			t.Error("Should return en error.")
		}

		filesLen, err := read()
		if err != nil {
			t.Error(err.Error())
		}

		if filesLen != 364 {
			t.Error("Wrong number of extracted entities.")
		}

		rcrd := new(Record)
		if err := rcrd.Destroy(); err == nil {
			t.Error("Should return an error.")
		}

		if errC := clean(); errC != nil {
			t.Error(errC.Error())
		}

	})

	t.Run("nonValidExportConf", func(t *testing.T) {

		def0 := ExportConfDefault()
		def0.Mode = 5
		ret := NewExport(def0)
		if ret != nil {
			t.Error("Should return nil Pst Export due to wrong Mode")
		}

		def1 := ExportConfDefault()
		def1.OutputMode = 5
		ret = NewExport(def1)
		if ret != nil {
			t.Error("Should return nil Pst Export due to wrong OutputMode")
		}

		def2 := ExportConfDefault()
		def2.ContactMode = 5
		ret = NewExport(def2)
		if ret != nil {
			t.Error("Should return nil Pst Export due to wrong ContactMode")
		}

		def3 := ExportConfDefault()
		def3.DeletedMode = 5
		ret = NewExport(def3)
		if ret != nil {
			t.Error("Should return nil Pst Export due to wrong DeletedMode")
		}

		def4 := ExportConfDefault()
		def4.FileNameLen = -2
		ret = NewExport(def4)
		if ret != nil {
			t.Error("Should return nil Pst Export due to wrong FileNameLen")
		}

	})

}
