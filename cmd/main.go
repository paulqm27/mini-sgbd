package main

import (
	"fmt"
	"mini-sgbd/internal/buffer"
	"mini-sgbd/internal/storage"
)

/* func main() {
	sm, err := storage.NewStorageManager("data/database.db")
	if err != nil {
		panic(err)
	}
	defer sm.Close()

	// Crear nueva página
	page := storage.NewPage()

	// Insertar varios registros
	page.InsertRecord([]byte("Registro 1"))
	page.InsertRecord([]byte("Registro 2"))
	page.InsertRecord([]byte("Registro 3"))
	page.InsertRecord([]byte("Registro 4"))
	page.InsertRecord([]byte("Registro 5"))

	// Guardar en disco
	err = sm.WritePageData(0, page)
	if err != nil {
		panic(err)
	}

	fmt.Println("✔ Página guardada en disco")

	// Leer desde disco
	loadedPage, err := sm.ReadPageData(0)
	if err != nil {
		panic(err)
	}

	records := loadedPage.ReadRecords()

	fmt.Println("\nRegistros leídos desde disco:")
	for i, r := range records {
		fmt.Printf("Record %d: %s\n", i+1, string(r))
	}

	fmt.Println("\nMetadata:")
	fmt.Println("NumRecords:", loadedPage.GetNumRecords())
	fmt.Println("FreeOffset:", loadedPage.GetFreeOffset())
}
*/

func main() {

	sm, err := storage.NewStorageManager("data/database_v3.db")
	if err != nil {
		panic(err)
	}
	defer sm.Close()

	bm := buffer.NewBufferManager(2, sm)

	// Página 0
	f1 := bm.FetchPage(0)
	f1.Page.InsertRecord([]byte("S3 - A"))
	f1.Page.InsertRecord([]byte("S3 - B"))
	bm.UnpinPage(0, true)

	// Página 1
	f2 := bm.FetchPage(1)
	f2.Page.InsertRecord([]byte("P1 - X"))
	bm.UnpinPage(1, true)

	// Página 2 (forza LRU)
	f3 := bm.FetchPage(2)
	f3.Page.InsertRecord([]byte("P2 - LRU"))
	bm.UnpinPage(2, true)

	bm.Flush()

	fmt.Println("✔ Buffer Manager OK")

	// verificación
	for i := 0; i < 3; i++ {
		p, _ := sm.ReadPageData(i)
		fmt.Println("\nPágina", i)

		if p != nil {
			for _, r := range p.ReadRecords() {
				fmt.Println(string(r))
			}
		}
	}
}
