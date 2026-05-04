package storage

import (
	"encoding/binary"
)

const PageSize = 4096

type Page struct {
	Data []byte
}

// Crear página siempre válida
func NewPage() *Page {
	p := &Page{
		Data: make([]byte, PageSize),
	}

	p.initHeader()
	return p
}

// Inicializa header seguro
func (p *Page) initHeader() {
	p.SetNumRecords(0)
	p.SetFreeOffset(8)
}

// -------- HEADER --------

func (p *Page) GetNumRecords() int {
	if p.Data == nil {
		return 0
	}
	return int(binary.LittleEndian.Uint32(p.Data[0:4]))
}

func (p *Page) SetNumRecords(n int) {
	binary.LittleEndian.PutUint32(p.Data[0:4], uint32(n))
}

func (p *Page) GetFreeOffset() int {
	if p.Data == nil {
		return 8
	}
	offset := int(binary.LittleEndian.Uint32(p.Data[4:8]))
	if offset < 8 {
		return 8
	}
	return offset
}

func (p *Page) SetFreeOffset(offset int) {
	binary.LittleEndian.PutUint32(p.Data[4:8], uint32(offset))
}

// -------- INSERT --------

func (p *Page) InsertRecord(record []byte) bool {
	if p.Data == nil {
		return false
	}

	free := p.GetFreeOffset()

	if free+len(record)+4 > PageSize {
		return false
	}

	binary.LittleEndian.PutUint32(p.Data[free:free+4], uint32(len(record)))
	copy(p.Data[free+4:], record)

	p.SetFreeOffset(free + 4 + len(record))
	p.SetNumRecords(p.GetNumRecords() + 1)

	return true
}

// -------- READ --------

func (p *Page) ReadRecords() [][]byte {
	if p.Data == nil {
		return nil
	}

	var records [][]byte
	offset := 8

	for i := 0; i < p.GetNumRecords(); i++ {
		if offset+4 > len(p.Data) {
			break
		}

		size := int(binary.LittleEndian.Uint32(p.Data[offset : offset+4]))
		offset += 4

		if offset+size > len(p.Data) {
			break
		}

		rec := make([]byte, size)
		copy(rec, p.Data[offset:offset+size])
		offset += size

		records = append(records, rec)
	}

	return records
}
