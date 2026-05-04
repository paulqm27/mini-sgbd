package storage

import (
	"encoding/binary"
)

const PageSize = 4096

type Page struct {
	Data []byte
}

// Crear página nueva
func NewPage() *Page {
	p := &Page{
		Data: make([]byte, PageSize),
	}

	p.SetNumRecords(0)
	p.SetFreeOffset(8) // header = 8 bytes

	return p
}

// ---------- HEADER ----------

// NumRecords (0-3)
func (p *Page) GetNumRecords() int {
	return int(binary.LittleEndian.Uint32(p.Data[0:4]))
}

func (p *Page) SetNumRecords(n int) {
	binary.LittleEndian.PutUint32(p.Data[0:4], uint32(n))
}

// FreeOffset (4-7)
func (p *Page) GetFreeOffset() int {
	return int(binary.LittleEndian.Uint32(p.Data[4:8]))
}

func (p *Page) SetFreeOffset(offset int) {
	binary.LittleEndian.PutUint32(p.Data[4:8], uint32(offset))
}

// ---------- INSERT ----------

func (p *Page) InsertRecord(record []byte) bool {
	free := p.GetFreeOffset()

	if free+len(record)+4 > PageSize {
		return false
	}

	// tamaño del registro
	binary.LittleEndian.PutUint32(p.Data[free:free+4], uint32(len(record)))

	// datos
	copy(p.Data[free+4:], record)

	p.SetFreeOffset(free + 4 + len(record))
	p.SetNumRecords(p.GetNumRecords() + 1)

	return true
}

// ---------- READ ----------

func (p *Page) ReadRecords() [][]byte {
	var records [][]byte

	offset := 8

	for i := 0; i < p.GetNumRecords(); i++ {
		size := int(binary.LittleEndian.Uint32(p.Data[offset : offset+4]))
		offset += 4

		rec := make([]byte, size)
		copy(rec, p.Data[offset:offset+size])
		offset += size

		records = append(records, rec)
	}

	return records
}
