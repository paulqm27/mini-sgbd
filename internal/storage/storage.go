package storage

import (
	"fmt"
	"os"
)

const PageSize = 4096 // 4KB

type StorageManager struct {
	file *os.File
}

// Crear o abrir archivo
func NewStorageManager(filename string) (*StorageManager, error) {
	file, err := os.OpenFile(filename, os.O_RDWR|os.O_CREATE, 0666)
	if err != nil {
		return nil, err
	}

	return &StorageManager{file: file}, nil
}

// Escribir una página en una posición
func (sm *StorageManager) WritePage(pageID int, data []byte) error {
	if len(data) > PageSize {
		return fmt.Errorf("data exceeds page size")
	}

	offset := int64(pageID * PageSize)

	_, err := sm.file.Seek(offset, 0)
	if err != nil {
		return err
	}

	page := make([]byte, PageSize)
	copy(page, data)

	_, err = sm.file.Write(page)
	return err
}

// Leer una página
func (sm *StorageManager) ReadPage(pageID int) ([]byte, error) {
	offset := int64(pageID * PageSize)

	_, err := sm.file.Seek(offset, 0)
	if err != nil {
		return nil, err
	}

	page := make([]byte, PageSize)
	_, err = sm.file.Read(page)
	if err != nil {
		return nil, err
	}

	return page, nil
}

// Cerrar archivo
func (sm *StorageManager) Close() {
	sm.file.Close()
}
