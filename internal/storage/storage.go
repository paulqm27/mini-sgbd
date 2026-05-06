package storage

import (
	"fmt"
	"io"
	"os"
)

// GestorStorage administra el acceso al archivo binario.
type GestorStorage struct {
	archivo *os.File
}

// NuevoGestorStorage abre o crea el archivo de base de datos.
func NuevoGestorStorage(nombreArchivo string) (*GestorStorage, error) {

	archivo, err := os.OpenFile(
		nombreArchivo,
		os.O_RDWR|os.O_CREATE,
		0666,
	)

	if err != nil {
		return nil, err
	}

	return &GestorStorage{
		archivo: archivo,
	}, nil
}

func (gs *GestorStorage) EscribirPagina(
	idPagina int,
	datos []byte,
) error {

	posicion := int64(idPagina * TamañoPagina)

	_, err := gs.archivo.WriteAt(
		datos,
		posicion,
	)

	return err
}

func (gs *GestorStorage) LeerPagina(
	idPagina int,
) ([]byte, error) {

	posicion := int64(idPagina * TamañoPagina)

	pagina := make([]byte, TamañoPagina)

	_, err := gs.archivo.ReadAt(
		pagina,
		posicion,
	)

	if err != nil && err != io.EOF {
		return nil, err
	}

	return pagina, nil
}

// Guarda un objeto Pagina completo.
func (gs *GestorStorage) EscribirDatosPagina(
	idPagina int,
	pagina *Pagina,
) error {

	if pagina == nil {
		return fmt.Errorf("la página es nil")
	}

	return gs.EscribirPagina(
		idPagina,
		pagina.Datos,
	)
}

// Recupera una página completa.
func (gs *GestorStorage) LeerDatosPagina(
	idPagina int,
) (*Pagina, error) {

	datos, err := gs.LeerPagina(idPagina)

	if err != nil {
		return nil, err
	}

	if datos == nil {
		return NuevaPagina(), nil
	}

	return &Pagina{
		Datos: datos,
	}, nil
}

func (gs *GestorStorage) Cerrar() {
	gs.archivo.Close()
}
