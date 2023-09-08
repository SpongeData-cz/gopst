package gopst_test

import (
	"fmt"
	"os"
	"testing"

	. "github.com/SpongeData-cz/gopst"
)

func TestPst(t *testing.T) {

	path := "./fixtures/"

	setup := func() error {
		if err := os.Mkdir(path+"out", os.ModePerm); err != nil {
			return err
		}
		return nil
	}

	read := func() (int, error) {
		files, err := os.ReadDir(path + "out")
		if err != nil {
			return 0, err
		}
		return len(files), nil
	}

	check_error := func(err int, i int) {
		switch err {
		case ERROR_NOT_UNIQUE_MSG_STORE:
			fmt.Printf("Record on index %d have \"ERROR_NOT_UNIQUE_MSG_STORE\" type of error\n", i)
		case ERROR_ROOT_NOT_FOUND:
			fmt.Printf("Record on index %d have \"ERROR_ROOT_NOT_FOUND\" type of error\n", i)
		case ERROR_OPEN:
			fmt.Printf("Record on index %d have \"ERROR_OPEN\" type of error\n", i)
		case ERROR_INDEX_LOAD:
			fmt.Printf("Record on index %d have \"ERROR_INDEX_LOAD\" type of error\n", i)
		case ERROR_UNKNOWN_RECORD:
			fmt.Printf("Record on index %d have \"ERROR_UNKNOWN_RECORD\" type of error\n", i)
		default:
			fmt.Printf("Record on index %d have uknown type of error\n", i)
		}
	}

	clean := func() error {
		if err := os.RemoveAll(path + "out"); err != nil {
			return err
		}
		return nil
	}

	t.Run("example", func(t *testing.T) {

		pst := NewPst(path + "sample.pst")
		if pst.NumError != NO_ERROR {
			t.Error(pst.LastError)
		}

		ret := NewExport(ExportConfDefault())
		if ret == nil {
			t.Error("Should return valid Pst Export")
		}

		if errS := setup(); errS != nil {
			t.Error(errS.Error())
		}

		records := pst.List()

		for i, curr := range records {
			newName := fmt.Sprintf("out/output_%d.eml", i)
			curr.SetRecordRenaming(path + newName)
			curr.RecordToFile(ret)
		}

		for i, curr := range records {
			if curr.Err != NO_ERROR {
				check_error(curr.Err, i)
			}
		}

		if err := pst.Destroy(); err != nil {
			t.Error(err.Error())
		}

		if err := DestroyList(records); err != nil {
			t.Error(err.Error())
		}

		if err := ret.Destroy(); err != nil {
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
		if err := ret.Destroy(); err != nil {
			t.Error(err.Error())
		}

		// Pst init
		pst := NewPst(path + "sample.pst")
		if pst.NumError != NO_ERROR {
			t.Error(pst.LastError)
		}
		if err := pst.Destroy(); err != nil {
			t.Error(err.Error())
		}

		// Non-existing path Pst init
		// TODO: Printing error on stderr, make it shhhh?
		pstNon := NewPst("./im/not/existing.pst")
		if pstNon.NumError != ERROR_OPEN {
			t.Error("Should throw ERROR_OPEN error.")
		}
		if err := pstNon.Destroy(); err != nil {
			t.Error(err.Error())
		}

	})

	t.Run("list", func(t *testing.T) {

		pst := NewPst(path + "sample.pst")
		if pst.NumError != NO_ERROR {
			t.Error(pst.LastError)
		}

		records := pst.List()

		if len(records) != 2 {
			t.Errorf("Expected 2 records, got: %d\n", len(records))
		}

		if err := pst.Destroy(); err != nil {
			t.Error(err.Error())
		}

		if err := DestroyList(records); err != nil {
			t.Error(err.Error())
		}

	})

	t.Run("renaming", func(t *testing.T) {

		pst := NewPst(path + "sample.pst")
		if pst.NumError != NO_ERROR {
			t.Error(pst.LastError)
		}

		records := pst.List()
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

		if err := pst.Destroy(); err != nil {
			t.Error(err.Error())
		}

		if err := DestroyList(records); err != nil {
			t.Error(err.Error())
		}
	})

	t.Run("extraction", func(t *testing.T) {

		pst := NewPst(path + "backup.pst")
		if pst.NumError != NO_ERROR {
			t.Error(pst.LastError)
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

		records := pst.List()
		if len(records) != 370 {
			t.Errorf("Expected 370 records, got %d\n", len(records))
		}

		for i, curr := range records {
			newName := fmt.Sprintf("out/output_%d.eml", i)
			curr.SetRecordRenaming(path + newName)

			written := curr.RecordToFile(ret)
			fmt.Printf("WRITTEN: %d\n", written)
		}

		for i, curr := range records {
			if (curr.Renaming == "./fixtures/out/output_56.eml" ||
				curr.Renaming == "./fixtures/out/output_73.eml" ||
				curr.Renaming == "./fixtures/out/output_325.eml" ||
				curr.Renaming == "./fixtures/out/output_366.eml" ||
				curr.Renaming == "./fixtures/out/output_367.eml" ||
				curr.Renaming == "./fixtures/out/output_369.eml") && (!curr.GetDir()) {
				t.Errorf("Record %s is not a dir, but it should be\n", curr.Renaming)
			}
			if curr.Err != NO_ERROR {
				check_error(curr.Err, i)
			}
		}

		if records[34].GetDir() {
			t.Error("Record on index 34 shouldn't be dir, but it is.\n")
		}

		for i, curr := range records {
			if curr.Err != NO_ERROR {
				check_error(curr.Err, i)
			}
		}

		if err := pst.Destroy(); err != nil {
			t.Error(err.Error())
		}

		if err := pst.Destroy(); err == nil {
			t.Error("Should return an error.")
		}

		if err := DestroyList(records); err != nil {
			t.Error(err.Error())
		}

		if err := DestroyList(records); err == nil {
			t.Error("Should return en error.")
		}

		if err := ret.Destroy(); err != nil {
			t.Error(err.Error())
		}

		if err := ret.Destroy(); err == nil {
			t.Error("Should return en error.")
		}

		filesLen, err := read()
		if err != nil {
			t.Error(err.Error())
		}

		if filesLen != 370 {
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
