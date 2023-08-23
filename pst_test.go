package gopst_test

import (
	"fmt"
	"os"
	"testing"

	. "github.com/SpongeData-cz/gopst"
)

func TestPst(t *testing.T) {

	printEnum := func(enum PstRecordEnumerator) {
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

		def := PstExporConfDefault()
		ret := NewPstExport(def)
		if ret == nil {
			t.Error("Should return valid Pst Export")
		}

		fmt.Printf("Return of new: %+v\n", ret)

	})

	t.Run("list", func(t *testing.T) {

		enum := PstList("./src/sample.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		printEnum(enum)

		// Non-existing path
		// TODO: Printing error on stdout/stderr, make it shhhh?
		enumNon := PstList("./im/not/existing.pst")
		if enumNon.NumError != ERROR_OPEN {
			t.Error("Should throw ERROR_OPEN error.")
		}

		printEnum(enumNon)

	})

	t.Run("renaming", func(t *testing.T) {
		path := "./src/"

		enum := PstList(path + "sample.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		printEnum(enum)

		records := enum.Items
		for i := 0; i < len(records); i++ {
			curr := records[i]
			newName := fmt.Sprintf("out/TvojeMamka%d.eml", i)
			curr.SetRecordRenaming(path + newName)
		}

		printEnum(enum)
	})

	t.Run("extraction", func(t *testing.T) {
		path := "./src/"

		enum := PstList(path + "backup.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		ret := NewPstExport(PstExporConfDefault())
		if ret == nil {
			t.Error("Should return valid Pst Export")
		}

		if errS := setup(); errS != nil {
			t.Error(errS.Error())
		}

		records := enum.Items
		for i := 0; i < len(records); i++ {
			curr := records[i]
			newName := fmt.Sprintf("out/TvojeMamkaVazi%dKg.eml", i)
			curr.SetRecordRenaming(path + newName)

			var err int
			written, errNum := PstRecordToFile(curr, ret, err)
			if errNum != NO_ERROR {
				t.Error("Something went wrong in RecordToFile function.")
			}
			if err != 0 {
				t.Errorf("There is error with %dnth record.", i)
			}

			fmt.Printf("Written: %d, error: %d\n", written, err)

		}

		if err := DestroyRecordEnumerator(&enum); err != nil {
			t.Error(err.Error())
		}

		if err := DestroyPstExport(ret); err != nil {
			t.Error(err.Error())
		}

		filesLen, err := read()
		if err != nil {
			t.Error(err.Error())
		}

		if filesLen != 364 {
			t.Error("Wrong number of extracted entities.")
		}

		if errC := clean(); errC != nil {
			t.Error(errC.Error())
		}

	})

}
