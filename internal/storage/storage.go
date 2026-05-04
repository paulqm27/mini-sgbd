package storage

import (
	"fmt"
	"io"
	"os"
)

type StorageManager struct {
	file *os.File
}

func NewStorageManager(filename string) (*StorageManager, error) {
	file, err := os.OpenFile(filename, os.O_RDWR|os.O_CREATE, 0666)
	if err != nil {
		return nil, err
	}

	return &StorageManager{file: file}, nil
}

// escritura segura
func (sm *StorageManager) WritePage(pageID int, data []byte) error {
	offset := int64(pageID * PageSize)

	_, err := sm.file.WriteAt(data, offset)
	return err
}

// lectura segura
func (sm *StorageManager) ReadPage(pageID int) ([]byte, error) {
	offset := int64(pageID * PageSize)

	page := make([]byte, PageSize)

	_, err := sm.file.ReadAt(page, offset)
	if err != nil && err != io.EOF {
		return nil, err
	}

	return page, nil
}

// wrapper seguro
func (sm *StorageManager) WritePageData(pageID int, page *Page) error {
	if page == nil {
		return fmt.Errorf("page is nil")
	}
	return sm.WritePage(pageID, page.Data)
}

func (sm *StorageManager) ReadPageData(pageID int) (*Page, error) {
	data, err := sm.ReadPage(pageID)
	if err != nil {
		return nil, err
	}

	if data == nil {
		return NewPage(), nil
	}

	return &Page{Data: data}, nil
}

func (sm *StorageManager) Close() {
	sm.file.Close()
}
