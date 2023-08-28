package gopst_test

import (
	"fmt"
	"os"
	"testing"

	. "github.com/SpongeData-cz/gopst"
)

func TestPst(t *testing.T) {

	printEnum := func(enum *RecordEnumerator) {
		println("\n--PRINTING ENUM--")
		for i, e := range enum.Items {
			fmt.Printf("Item number %d: %+v\n", i, e)
			fmt.Printf("	TypeOfRecord: %d\n	LogicalPath: %s\n	Name: %s\n	Renaming: %s\n	ExtraMImeHeaders: %s\n\n",
				e.TypeOfRecord,
				// e.PFile,
				// e.PItem,
				e.LogicalPath,
				e.Name,
				e.Renaming,
				e.ExtraMimeHeaders,
			)
		}
		fmt.Printf(
			"Capacity: %d\nUsed: %d\nFile: %+v\nLastError: %s\nNumError: %d\n",
			enum.Capacity,
			enum.Used,
			enum.File,
			enum.LastError,
			enum.NumError,
		)
		println("\n")
	}

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

	t.Run("init", func(t *testing.T) {

		def := ExportConfDefault()
		ret := NewExport(def)
		if ret == nil {
			t.Error("Should return valid Pst Export")
		}

		fmt.Printf("Return of new: %+v\n", ret)

	})

	t.Run("list", func(t *testing.T) {

		enum := List("./src/sample.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		printEnum(enum)

		// Non-existing path
		// TODO: Printing error on stdout/stderr, make it shhhh?
		enumNon := List("./im/not/existing.pst")
		if enumNon.NumError != ERROR_OPEN {
			t.Error("Should throw ERROR_OPEN error.")
		}

		printEnum(enumNon)

	})

	t.Run("renaming", func(t *testing.T) {
		path := "./src/"

		enum := List(path + "sample.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		printEnum(enum)

		records := enum.Items
		for i := 0; i < len(records); i++ {
			curr := records[i]
			newName := fmt.Sprintf("out/output_%d.eml", i)
			curr.SetRecordRenaming(path + newName)
		}

		printEnum(enum)
	})

	t.Run("extraction", func(t *testing.T) {
		path := "./src/"

		enum := List(path + "backup.pst")
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

		records := enum.Items
		for i := 0; i < len(records); i++ {
			curr := records[i]
			newName := fmt.Sprintf("out/output_%d.eml", i)
			curr.SetRecordRenaming(path + newName)

			var err int
			written, errNum := curr.RecordToFile(ret, err)
			if errNum != NO_ERROR {
				t.Error("Something went wrong in RecordToFile function.")
			}
			if err != 0 {
				t.Errorf("There is error with %dnth record.", i)
			}

			fmt.Printf("Written: %d, error: %d\n", written, err)

		}

		if err := enum.DestroyRecordEnumerator(); err != nil {
			t.Error(err.Error())
		}

		if err := enum.DestroyRecordEnumerator(); err == nil {
			t.Error("Should return an error.")
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
		if err := rcrd.DestroyRecord(); err == nil {
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
			t.Error("Should return nil Pst Export")
		}

		def1 := ExportConfDefault()
		def1.OutputMode = 5
		ret = NewExport(def1)
		if ret != nil {
			t.Error("Should return nil Pst Export")
		}

		def2 := ExportConfDefault()
		def2.ContactMode = 5
		ret = NewExport(def2)
		if ret != nil {
			t.Error("Should return nil Pst Export")
		}

		def3 := ExportConfDefault()
		def3.DeletedMode = 5
		ret = NewExport(def3)
		if ret != nil {
			t.Error("Should return nil Pst Export")
		}

		def4 := ExportConfDefault()
		def4.FileNameLen = -2
		ret = NewExport(def4)
		if ret != nil {
			t.Error("Should return nil Pst Export")
		}

	})

	t.Run("validEnumDestroyedRcrd", func(t *testing.T) {
		path := "./src/"

		enum := List(path + "backup.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		record := enum.Items[1]
		if err := record.DestroyRecord(); err != nil {
			t.Error(err.Error())
		}

		if err := enum.DestroyRecordEnumerator(); err == nil {
			t.Error("Should return an error.")
		}

	})

}
