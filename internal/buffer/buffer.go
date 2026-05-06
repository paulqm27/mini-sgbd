package buffer

import "mini-sgbd/internal/storage"

// Frame -> página cargada en memoria RAM
type Frame struct {
	IDPagina int
	Pagina   *storage.Pagina
	PinCount int
	Dirty    bool
}

// Administra estas paginas
type BufferManager struct {
	capacidad int
	frames    map[int]*Frame
	ordenLRU  []int
	storage   *storage.GestorStorage
}

// Nuevo Buffer Pool.
func NuevoBufferManager(
	capacidad int,
	gestorStorage *storage.GestorStorage,
) *BufferManager {

	return &BufferManager{
		capacidad: capacidad,
		frames:    make(map[int]*Frame),
		ordenLRU:  []int{},
		storage:   gestorStorage,
	}
}

// actualizarLRU mueve la página utilizada al final de la lista LRU.
func (bm *BufferManager) actualizarLRU(idPagina int) {

	for i, valor := range bm.ordenLRU {

		if valor == idPagina {

			bm.ordenLRU = append(
				bm.ordenLRU[:i],
				bm.ordenLRU[i+1:]...,
			)

			break
		}
	}

	bm.ordenLRU = append(
		bm.ordenLRU,
		idPagina,
	)
}

// Elimina una página usando la política LRU.
func (bm *BufferManager) reemplazarPagina() {

	for _, idPagina := range bm.ordenLRU {

		frame := bm.frames[idPagina]

		// Solo puede eliminarse si no está siendo usada
		if frame.PinCount == 0 {

			if frame.Dirty {

				bm.storage.EscribirDatosPagina(
					idPagina,
					frame.Pagina,
				)
			}

			delete(bm.frames, idPagina)

			bm.ordenLRU = bm.ordenLRU[1:]

			return
		}
	}
}

// Recuperar una página desde memoria
func (bm *BufferManager) ObtenerPagina(
	idPagina int,
) *Frame {

	if frame, existe := bm.frames[idPagina]; existe {

		frame.PinCount++

		bm.actualizarLRU(idPagina)

		return frame
	}

	// Si buffer lleno, se aplica reemplazo LRU
	if len(bm.frames) >= bm.capacidad {

		bm.reemplazarPagina()
	}

	pagina, _ := bm.storage.LeerDatosPagina(idPagina)

	if pagina == nil {

		pagina = storage.NuevaPagina()
	}

	frame := &Frame{
		IDPagina: idPagina,
		Pagina:   pagina,
		PinCount: 1,
	}

	bm.frames[idPagina] = frame

	bm.actualizarLRU(idPagina)

	return frame
}

// Reduce el contador PinCount, marca la página como dirty si fue modificada
func (bm *BufferManager) LiberarPagina(
	idPagina int,
	dirty bool,
) {

	if frame, existe := bm.frames[idPagina]; existe {

		frame.PinCount--

		if dirty {
			frame.Dirty = true
		}
	}
}

// Escribe todas las páginas dirty nuevamente a disco.
func (bm *BufferManager) Flush() {

	for idPagina, frame := range bm.frames {

		if frame.Dirty {

			bm.storage.EscribirDatosPagina(
				idPagina,
				frame.Pagina,
			)
		}
	}
}
