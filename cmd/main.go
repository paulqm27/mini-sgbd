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

	// Datos de prueba
	data := []byte("Hola, esto es una prueba de pagina")

	// Escribir en página 0
	err = sm.WritePage(0, data)
	if err != nil {
		panic(err)
	}

	fmt.Println("Página escrita correctamente")

	// Leer página 0
	page, err := sm.ReadPage(0)
	if err != nil {
		panic(err)
	}

	fmt.Println("Contenido leído:")
	fmt.Println(string(page[:len(data)]))
}
