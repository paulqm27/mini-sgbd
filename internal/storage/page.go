package storage

import (
	"encoding/binary"
)

const TamañoPagina = 4096

type Pagina struct {
	Datos []byte
}

func NuevaPagina() *Pagina {

	p := &Pagina{
		Datos: make([]byte, TamañoPagina),
	}

	p.inicializarHeader()

	return p
}

func (p *Pagina) inicializarHeader() {

	p.EstablecerNumeroRegistros(0)
	p.EstablecerPosicionLibre(8)
}

// ========================================
// HEADER
// ========================================

func (p *Pagina) ObtenerNumeroRegistros() int {

	if p.Datos == nil {
		return 0
	}

	return int(binary.LittleEndian.Uint32(p.Datos[0:4]))
}

// Actualiza la cantidad de registros.
func (p *Pagina) EstablecerNumeroRegistros(n int) {

	binary.LittleEndian.PutUint32(
		p.Datos[0:4],
		uint32(n),
	)
}

// Devuelve la siguiente posición libre disponible.
func (p *Pagina) ObtenerPosicionLibre() int {

	if p.Datos == nil {
		return 8
	}

	posicion := int(
		binary.LittleEndian.Uint32(
			p.Datos[4:8],
		),
	)

	if posicion < 8 {
		return 8
	}

	return posicion
}

// Actualiza la siguiente posición libre.
func (p *Pagina) EstablecerPosicionLibre(posicion int) {

	binary.LittleEndian.PutUint32(
		p.Datos[4:8],
		uint32(posicion),
	)
}

// ========================================
// INSERTAR REGISTRO
// ========================================

func (p *Pagina) InsertarRegistro(registro []byte) bool {

	if p.Datos == nil {
		return false
	}

	posicionLibre := p.ObtenerPosicionLibre()

	// Verificar espacio disponible
	if posicionLibre+len(registro)+4 > TamañoPagina {
		return false
	}

	// Guardar tamaño del registro
	binary.LittleEndian.PutUint32(
		p.Datos[posicionLibre:posicionLibre+4],
		uint32(len(registro)),
	)

	// Guardar contenido del registro
	copy(
		p.Datos[posicionLibre+4:],
		registro,
	)

	p.EstablecerPosicionLibre(
		posicionLibre + 4 + len(registro),
	)

	p.EstablecerNumeroRegistros(
		p.ObtenerNumeroRegistros() + 1,
	)

	return true
}

// ========================================
// LEER REGISTROS
// ========================================

func (p *Pagina) LeerRegistros() [][]byte {

	if p.Datos == nil {
		return nil
	}

	var registros [][]byte

	posicion := 8

	for i := 0; i < p.ObtenerNumeroRegistros(); i++ {

		// Verificar lectura segura
		if posicion+4 > len(p.Datos) {
			break
		}

		// Leer tamaño del registro
		tamaño := int(
			binary.LittleEndian.Uint32(
				p.Datos[posicion : posicion+4],
			),
		)

		posicion += 4

		// Verificar límites
		if posicion+tamaño > len(p.Datos) {
			break
		}

		// Copiar registro
		registro := make([]byte, tamaño)

		copy(
			registro,
			p.Datos[posicion:posicion+tamaño],
		)

		posicion += tamaño

		registros = append(registros, registro)
	}

	return registros
}
