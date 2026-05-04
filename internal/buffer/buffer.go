package buffer

import "mini-sgbd/internal/storage"

type Frame struct {
	PageID   int
	Page     *storage.Page
	PinCount int
	Dirty    bool
}

type BufferManager struct {
	capacity int
	frames   map[int]*Frame
	order    []int
	storage  *storage.StorageManager
}

func NewBufferManager(capacity int, sm *storage.StorageManager) *BufferManager {
	return &BufferManager{
		capacity: capacity,
		frames:   make(map[int]*Frame),
		order:    []int{},
		storage:  sm,
	}
}

func (bm *BufferManager) touch(id int) {
	for i, v := range bm.order {
		if v == id {
			bm.order = append(bm.order[:i], bm.order[i+1:]...)
			break
		}
	}
	bm.order = append(bm.order, id)
}

func (bm *BufferManager) evict() {
	for _, id := range bm.order {
		frame := bm.frames[id]

		if frame.PinCount == 0 {
			if frame.Dirty {
				bm.storage.WritePageData(id, frame.Page)
			}
			delete(bm.frames, id)
			bm.order = bm.order[1:]
			return
		}
	}
}

func (bm *BufferManager) FetchPage(pageID int) *Frame {

	if frame, ok := bm.frames[pageID]; ok {
		frame.PinCount++
		bm.touch(pageID)
		return frame
	}

	if len(bm.frames) >= bm.capacity {
		bm.evict()
	}

	page, _ := bm.storage.ReadPageData(pageID)

	if page == nil {
		page = storage.NewPage()
	}

	frame := &Frame{
		PageID:   pageID,
		Page:     page,
		PinCount: 1,
	}

	bm.frames[pageID] = frame
	bm.touch(pageID)

	return frame
}

func (bm *BufferManager) UnpinPage(pageID int, dirty bool) {
	if frame, ok := bm.frames[pageID]; ok {
		frame.PinCount--
		if dirty {
			frame.Dirty = true
		}
	}
}

func (bm *BufferManager) Flush() {
	for id, frame := range bm.frames {
		if frame.Dirty {
			bm.storage.WritePageData(id, frame.Page)
		}
	}
}
