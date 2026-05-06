package main

import (
	"fmt"
	"mini-sgbd/internal/buffer"
	"mini-sgbd/internal/storage"
)

func main() {

	// ========================================
	// SEMANA 2
	// Persistencia básica en archivos binarios
	// ========================================

	fmt.Println("===== SEMANA 2 =====")
	fmt.Println("Persistencia básica en disco")

	// Crear o abrir archivo de base de datos
	gestorStorage, err := storage.NuevoGestorStorage(
		"data/database.db",
	)

	if err != nil {
		panic(err)
	}

	defer gestorStorage.Cerrar()

	// Crear una nueva página
	pagina := storage.NuevaPagina()

	// Insertar registros
	pagina.InsertarRegistro([]byte("Juan"))
	pagina.InsertarRegistro([]byte("Pedro"))
	pagina.InsertarRegistro([]byte("Maria"))

	// Guardar página en disco
	err = gestorStorage.EscribirDatosPagina(
		0,
		pagina,
	)

	if err != nil {
		panic(err)
	}

	fmt.Println("✔ Página guardada correctamente en disco")

	// Leer nuevamente desde disco
	paginaLeida, err := gestorStorage.LeerDatosPagina(0)

	if err != nil {
		panic(err)
	}

	fmt.Println("\nRegistros recuperados desde disco:")

	for _, registro := range paginaLeida.LeerRegistros() {

		fmt.Println("-", string(registro))
	}

	// ========================================
	// SEMANA 3
	// Buffer Manager + Política LRU
	// ========================================

	fmt.Println("\n===== SEMANA 3 =====")
	fmt.Println("Buffer Manager con política LRU")

	// Crear Buffer Manager
	// Capacidad máxima: 2 páginas
	bufferManager := buffer.NuevoBufferManager(
		2,
		gestorStorage,
	)

	// ========================================
	// Página 0
	// ========================================

	frame1 := bufferManager.ObtenerPagina(0)

	frame1.Pagina.InsertarRegistro(
		[]byte("S3 - Registro A"),
	)

	frame1.Pagina.InsertarRegistro(
		[]byte("S3 - Registro B"),
	)

	// Liberar página y marcarla como dirty
	bufferManager.LiberarPagina(0, true)

	fmt.Println("✔ Página 0 cargada en buffer")

	// ========================================
	// Página 1
	// ========================================

	frame2 := bufferManager.ObtenerPagina(1)

	frame2.Pagina.InsertarRegistro(
		[]byte("Pagina 1 - Dato X"),
	)

	bufferManager.LiberarPagina(1, true)

	fmt.Println("✔ Página 1 cargada en buffer")

	// ========================================
	// Página 2
	// Esto obliga al sistema a aplicar LRU
	// ========================================

	frame3 := bufferManager.ObtenerPagina(2)

	frame3.Pagina.InsertarRegistro(
		[]byte("Pagina 2 - LRU"),
	)

	bufferManager.LiberarPagina(2, true)

	fmt.Println("✔ Página 2 cargada")
	fmt.Println("✔ Política LRU ejecutada")

	// Guardar páginas dirty nuevamente a disco
	bufferManager.Flush()

	fmt.Println("✔ Buffer Manager finalizado")

	// ========================================
	// VERIFICACIÓN FINAL
	// ========================================

	fmt.Println("\n===== VERIFICACIÓN =====")

	for i := 0; i < 3; i++ {

		pagina, _ := gestorStorage.LeerDatosPagina(i)

		fmt.Println("\nPágina", i)

		if pagina != nil {

			for _, registro := range pagina.LeerRegistros() {

				fmt.Println("-", string(registro))
			}
		}
	}
}
