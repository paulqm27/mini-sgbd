package main

import (
	"fmt"
	"mini-sgbd/internal/storage"
)

func main() {
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
