package storage

import (
	"fmt"
	"os"
)

type StorageManager struct {
	file *os.File
}

// Crear o abrir DB
func NewStorageManager(filename string) (*StorageManager, error) {
	file, err := os.OpenFile(filename, os.O_RDWR|os.O_CREATE, 0666)
	if err != nil {
		return nil, err
	}

	return &StorageManager{file: file}, nil
}

// Escribir página cruda
func (sm *StorageManager) WritePage(pageID int, data []byte) error {
	if len(data) > PageSize {
		return fmt.Errorf("data exceeds page size")
	}

	offset := int64(pageID * PageSize)

	_, err := sm.file.WriteAt(data, offset)
	return err
}

// Leer página cruda
func (sm *StorageManager) ReadPage(pageID int) ([]byte, error) {
	offset := int64(pageID * PageSize)

	page := make([]byte, PageSize)

	_, err := sm.file.ReadAt(page, offset)
	if err != nil {
		return nil, err
	}

	return page, nil
}

// Escribir Page
func (sm *StorageManager) WritePageData(pageID int, page *Page) error {
	return sm.WritePage(pageID, page.Data)
}

// Leer Page
func (sm *StorageManager) ReadPageData(pageID int) (*Page, error) {
	data, err := sm.ReadPage(pageID)
	if err != nil {
		return nil, err
	}

	return &Page{Data: data}, nil
}

// Cerrar archivo
func (sm *StorageManager) Close() {
	sm.file.Close()
}
