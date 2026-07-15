#include "bplus_tree.h"
#include <iostream>
#include <algorithm>
#include <cstring>

namespace index_m {

    // =========================================================
    // METODOS DE BPlusTreeNode
    // =========================================================

    bool BPlusTreeNode::IsLeaf() const {
        uint8_t val;
        page_->ReadRaw(0, &val, 1);
        return val != 0;
    }

    void BPlusTreeNode::SetIsLeaf(bool is_leaf) {
        uint8_t val = is_leaf ? 1 : 0;
        page_->WriteRaw(0, &val, 1);
    }

    uint16_t BPlusTreeNode::GetNumKeys() const {
        uint16_t val;
        page_->ReadRaw(1, &val, 2);
        return val;
    }

    void BPlusTreeNode::SetNumKeys(uint16_t num_keys) {
        page_->WriteRaw(1, &num_keys, 2);
    }

    int32_t BPlusTreeNode::GetParentPageId() const {
        int32_t val;
        page_->ReadRaw(3, &val, 4);
        return val;
    }

    void BPlusTreeNode::SetParentPageId(int32_t parent_page_id) {
        page_->WriteRaw(3, &parent_page_id, 4);
    }

    int32_t BPlusTreeNode::GetNextPageId() const {
        int32_t val;
        page_->ReadRaw(7, &val, 4);
        return val;
    }

    void BPlusTreeNode::SetNextPageId(int32_t next_page_id) {
        page_->WriteRaw(7, &next_page_id, 4);
    }

    int32_t BPlusTreeNode::GetPrevPageId() const {
        int32_t val;
        page_->ReadRaw(11, &val, 4);
        return val;
    }

    void BPlusTreeNode::SetPrevPageId(int32_t prev_page_id) {
        page_->WriteRaw(11, &prev_page_id, 4);
    }

    int32_t BPlusTreeNode::GetKey(int idx) const {
        int32_t key;
        page_->ReadRaw(16 + idx * 4, &key, 4);
        return key;
    }

    void BPlusTreeNode::SetKey(int idx, int32_t key) {
        page_->WriteRaw(16 + idx * 4, &key, 4);
    }

    RID BPlusTreeNode::GetRID(int idx, int max_keys) const {
        RID rid;
        page_->ReadRaw(16 + 4 * max_keys + idx * sizeof(RID), &rid, sizeof(RID));
        return rid;
    }

    void BPlusTreeNode::SetRID(int idx, int max_keys, const RID& rid) {
        page_->WriteRaw(16 + 4 * max_keys + idx * sizeof(RID), &rid, sizeof(RID));
    }

    int32_t BPlusTreeNode::GetChild(int idx, int max_keys) const {
        int32_t child;
        page_->ReadRaw(16 + 4 * max_keys + idx * 4, &child, 4);
        return child;
    }

    void BPlusTreeNode::SetChild(int idx, int max_keys, int32_t child_page_id) {
        page_->WriteRaw(16 + 4 * max_keys + idx * 4, &child_page_id, 4);
    }


    // =========================================================
    // METODOS DE BPlusTree
    // =========================================================

    BPlusTree::BPlusTree(buffer::BufferManager* bufferManager, storage::StorageManager* storageManager, int maxKeysLeaf, int maxKeysInternal)
        : bufferManager_(bufferManager), storageManager_(storageManager), rootPageId_(-1), maxKeysLeaf_(maxKeysLeaf), maxKeysInternal_(maxKeysInternal) {
        if (storageManager_->GetNumPages() == 0) {
            std::vector<uint8_t> emptyData(storage::PAGE_SIZE, 0);
            storageManager_->WritePage(METADATA_PAGE_ID, emptyData);
        }
        LoadRootPageId();
    }

    int BPlusTree::AllocatePage() {
        // GetNumPages() solo se actualiza al escribir, por lo que este ID es
        // siempre el siguiente disponible y jamás colisiona con páginas de datos.
        int newPageId = storageManager_->GetNumPages();
        std::vector<uint8_t> emptyData(storage::PAGE_SIZE, 0);
        bool ok = storageManager_->WritePage(newPageId, emptyData);
        if (!ok) {
            std::cerr << "Error crítico: No se pudo reservar la pagina " << newPageId << " en disco." << std::endl;
        }
        return newPageId;
    }

    // CRITICO: Búsqueda exacta desde la raíz hasta las hojas
    void BPlusTree::LoadRootPageId() {
        if (storageManager_->GetNumPages() == 0) {
            rootPageId_ = -1;
            return;
        }

        int32_t storedRoot = ReadMetadataRootPageId();
        if (storedRoot >= 0) {
            rootPageId_ = storedRoot;
        } else {
            rootPageId_ = -1;
        }
    }

    int32_t BPlusTree::ReadMetadataRootPageId() {
        auto frame = bufferManager_->GetPage(METADATA_PAGE_ID);
        uint8_t magic = 0;
        frame->page->ReadRaw(0, &magic, 1);
        int32_t rootId = -1;
        if (magic == 0xB5) {
            frame->page->ReadRaw(1, &rootId, sizeof(rootId));
        }
        bufferManager_->ReleasePage(METADATA_PAGE_ID, false);
        return rootId;
    }

    void BPlusTree::WriteMetadataRootPageId(int32_t rootPageId) {
        auto frame = bufferManager_->GetPage(METADATA_PAGE_ID);
        uint8_t magic = 0xB5;
        frame->page->WriteRaw(0, &magic, 1);
        frame->page->WriteRaw(1, &rootPageId, sizeof(rootPageId));
        bufferManager_->ReleasePage(METADATA_PAGE_ID, true);
    }

    void BPlusTree::PersistRootPageId() {
        WriteMetadataRootPageId(rootPageId_);
    }

    int BPlusTree::GetMinKeys(bool is_leaf) const {
        if (is_leaf) {
            return (maxKeysLeaf_ + 1) / 2;
        }
        int maxChildren = maxKeysInternal_ + 1;
        int minChildren = (maxChildren + 1) / 2;
        return minChildren - 1;
    }

    int BPlusTree::FindChildIndex(BPlusTreeNode& parent, int childPageId) const {
        int numKeys = parent.GetNumKeys();
        for (int i = 0; i <= numKeys; ++i) {
            if (parent.GetChild(i, maxKeysInternal_) == childPageId) {
                return i;
            }
        }
        return -1;
    }

    bool BPlusTree::Delete(int key) {
        if (rootPageId_ == -1) {
            return false;
        }

        int currentPageId = rootPageId_;
        buffer::Frame* frame = bufferManager_->GetPage(currentPageId);
        BPlusTreeNode node(frame->page.get());

        while (!node.IsLeaf()) {
            int numKeys = node.GetNumKeys();
            int nextChildIdx = numKeys;
            for (int i = 0; i < numKeys; ++i) {
                if (key < node.GetKey(i)) {
                    nextChildIdx = i;
                    break;
                }
            }
            int nextPageId = node.GetChild(nextChildIdx, maxKeysInternal_);
            bufferManager_->ReleasePage(currentPageId, false);
            currentPageId = nextPageId;
            frame = bufferManager_->GetPage(currentPageId);
            node = BPlusTreeNode(frame->page.get());
        }

        bufferManager_->ReleasePage(currentPageId, false);
        bool removed = DeleteEntryFromLeaf(currentPageId, key);
        if (removed) {
            PersistRootPageId();
        }
        return removed;
    }

    bool BPlusTree::DeleteEntryFromLeaf(int pageId, int key) {
        buffer::Frame* frame = bufferManager_->GetPage(pageId);
        BPlusTreeNode node(frame->page.get());
        int numKeys = node.GetNumKeys();
        int deleteIdx = -1;
        for (int i = 0; i < numKeys; ++i) {
            if (node.GetKey(i) == key) {
                deleteIdx = i;
                break;
            }
        }

        if (deleteIdx == -1) {
            bufferManager_->ReleasePage(pageId, false);
            return false;
        }

        for (int i = deleteIdx; i < numKeys - 1; ++i) {
            node.SetKey(i, node.GetKey(i + 1));
            node.SetRID(i, maxKeysLeaf_, node.GetRID(i + 1, maxKeysLeaf_));
        }
        node.SetNumKeys(numKeys - 1);
        bufferManager_->ReleasePage(pageId, true);

        if (pageId == rootPageId_) {
            if (numKeys - 1 == 0) {
                rootPageId_ = -1;
            }
            return true;
        }

        int minKeys = GetMinKeys(true);
        if (numKeys - 1 < minKeys) {
            return HandleUnderflow(pageId);
        }
        return true;
    }

    bool BPlusTree::HandleUnderflow(int pageId) {
        buffer::Frame* frame = bufferManager_->GetPage(pageId);
        BPlusTreeNode node(frame->page.get());
        bool isLeaf = node.IsLeaf();
        int parentPageId = node.GetParentPageId();
        bufferManager_->ReleasePage(pageId, false);

        if (parentPageId == -1) {
            return true;
        }

        buffer::Frame* parentFrame = bufferManager_->GetPage(parentPageId);
        BPlusTreeNode parentNode(parentFrame->page.get());
        int childIndex = FindChildIndex(parentNode, pageId);
        if (childIndex == -1) {
            bufferManager_->ReleasePage(parentPageId, false);
            return false;
        }

        int leftPageId = -1;
        int rightPageId = -1;
        int separatorIdx = -1;
        if (childIndex > 0) {
            leftPageId = parentNode.GetChild(childIndex - 1, maxKeysInternal_);
            rightPageId = pageId;
            separatorIdx = childIndex - 1;
        } else {
            leftPageId = pageId;
            rightPageId = parentNode.GetChild(childIndex + 1, maxKeysInternal_);
            separatorIdx = childIndex;
        }

        // Intentar redistribuir antes de fusionar
        int minKeys = isLeaf ? GetMinKeys(true) : GetMinKeys(false);
        int leftKeys = -1;
        int rightKeys = -1;
        if (leftPageId != pageId) {
            buffer::Frame* leftFrame = bufferManager_->GetPage(leftPageId);
            BPlusTreeNode leftNode(leftFrame->page.get());
            leftKeys = leftNode.GetNumKeys();
            bufferManager_->ReleasePage(leftPageId, false);
        }
        if (rightPageId != pageId) {
            buffer::Frame* rightFrame = bufferManager_->GetPage(rightPageId);
            BPlusTreeNode rightNode(rightFrame->page.get());
            rightKeys = rightNode.GetNumKeys();
            bufferManager_->ReleasePage(rightPageId, false);
        }

        bool redistributed = false;
        if (leftPageId != pageId && leftKeys > minKeys) {
            redistributed = isLeaf
                ? RedistributeLeafNodes(pageId, leftPageId, parentPageId, separatorIdx, true)
                : RedistributeInternalNodes(pageId, leftPageId, parentPageId, separatorIdx, true);
        } else if (rightPageId != pageId && rightKeys > minKeys) {
            redistributed = isLeaf
                ? RedistributeLeafNodes(pageId, rightPageId, parentPageId, separatorIdx, false)
                : RedistributeInternalNodes(pageId, rightPageId, parentPageId, separatorIdx, false);
        }

        if (redistributed) {
            bufferManager_->ReleasePage(parentPageId, false);
            return true;
        }

        bufferManager_->ReleasePage(parentPageId, false);

        if (leftPageId == -1 || rightPageId == -1) {
            return true;
        }

        if (isLeaf) {
            MergeLeafNodes(leftPageId, rightPageId, parentPageId, separatorIdx);
        } else {
            MergeInternalNodes(leftPageId, rightPageId, parentPageId, separatorIdx);
        }

        return true;
    }

    void BPlusTree::RemoveParentEntry(int parentPageId, int separatorIdx) {
        buffer::Frame* parentFrame = bufferManager_->GetPage(parentPageId);
        BPlusTreeNode parentNode(parentFrame->page.get());
        int numKeys = parentNode.GetNumKeys();

        for (int i = separatorIdx; i < numKeys - 1; ++i) {
            parentNode.SetKey(i, parentNode.GetKey(i + 1));
        }
        for (int i = separatorIdx + 1; i < numKeys; ++i) {
            parentNode.SetChild(i, maxKeysInternal_, parentNode.GetChild(i + 1, maxKeysInternal_));
        }
        parentNode.SetNumKeys(numKeys - 1);

        int updatedNumKeys = parentNode.GetNumKeys();
        if (parentPageId == rootPageId_) {
            if (updatedNumKeys == 0) {
                int newRootId = parentNode.GetChild(0, maxKeysInternal_);
                if (newRootId != -1) {
                    rootPageId_ = newRootId;
                    buffer::Frame* childFrame = bufferManager_->GetPage(newRootId);
                    BPlusTreeNode childNode(childFrame->page.get());
                    childNode.SetParentPageId(-1);
                    bufferManager_->ReleasePage(newRootId, true);
                } else {
                    rootPageId_ = -1;
                }
            }
            bufferManager_->ReleasePage(parentPageId, true);
        } else {
            bufferManager_->ReleasePage(parentPageId, true);
            int minKeys = GetMinKeys(false);
            if (updatedNumKeys < minKeys) {
                HandleUnderflow(parentPageId);
            }
        }
    }

    void BPlusTree::MergeLeafNodes(int leftPageId, int rightPageId, int parentPageId, int separatorIdx) {
        buffer::Frame* leftFrame = bufferManager_->GetPage(leftPageId);
        buffer::Frame* rightFrame = bufferManager_->GetPage(rightPageId);
        BPlusTreeNode leftNode(leftFrame->page.get());
        BPlusTreeNode rightNode(rightFrame->page.get());

        int leftNum = leftNode.GetNumKeys();
        int rightNum = rightNode.GetNumKeys();

        for (int i = 0; i < rightNum; ++i) {
            leftNode.SetKey(leftNum + i, rightNode.GetKey(i));
            leftNode.SetRID(leftNum + i, maxKeysLeaf_, rightNode.GetRID(i, maxKeysLeaf_));
        }
        leftNode.SetNumKeys(leftNum + rightNum);

        leftNode.SetNextPageId(rightNode.GetNextPageId());
        if (rightNode.GetNextPageId() != -1) {
            buffer::Frame* nextFrame = bufferManager_->GetPage(rightNode.GetNextPageId());
            BPlusTreeNode nextNode(nextFrame->page.get());
            nextNode.SetPrevPageId(leftPageId);
            bufferManager_->ReleasePage(rightNode.GetNextPageId(), true);
        }

        bufferManager_->ReleasePage(leftPageId, true);
        bufferManager_->ReleasePage(rightPageId, true);

        RemoveParentEntry(parentPageId, separatorIdx);
    }

    bool BPlusTree::RedistributeLeafNodes(int underflowPageId, int siblingPageId, int parentPageId, int separatorIdx, bool borrowFromLeft) {
        buffer::Frame* underFrame = bufferManager_->GetPage(underflowPageId);
        BPlusTreeNode underNode(underFrame->page.get());
        buffer::Frame* siblingFrame = bufferManager_->GetPage(siblingPageId);
        BPlusTreeNode siblingNode(siblingFrame->page.get());
        buffer::Frame* parentFrame = bufferManager_->GetPage(parentPageId);
        BPlusTreeNode parentNode(parentFrame->page.get());

        int underNum = underNode.GetNumKeys();
        int sibNum = siblingNode.GetNumKeys();
        if (borrowFromLeft) {
            for (int i = underNum; i > 0; --i) {
                underNode.SetKey(i, underNode.GetKey(i - 1));
                underNode.SetRID(i, maxKeysLeaf_, underNode.GetRID(i - 1, maxKeysLeaf_));
            }
            underNode.SetKey(0, siblingNode.GetKey(sibNum - 1));
            underNode.SetRID(0, maxKeysLeaf_, siblingNode.GetRID(sibNum - 1, maxKeysLeaf_));
            underNode.SetNumKeys(underNum + 1);
            siblingNode.SetNumKeys(sibNum - 1);
            parentNode.SetKey(separatorIdx, underNode.GetKey(0));
        } else {
            underNode.SetKey(underNum, siblingNode.GetKey(0));
            underNode.SetRID(underNum, maxKeysLeaf_, siblingNode.GetRID(0, maxKeysLeaf_));
            underNode.SetNumKeys(underNum + 1);
            for (int i = 0; i < sibNum - 1; ++i) {
                siblingNode.SetKey(i, siblingNode.GetKey(i + 1));
                siblingNode.SetRID(i, maxKeysLeaf_, siblingNode.GetRID(i + 1, maxKeysLeaf_));
            }
            siblingNode.SetNumKeys(sibNum - 1);
            parentNode.SetKey(separatorIdx, siblingNode.GetKey(0));
        }

        bufferManager_->ReleasePage(underflowPageId, true);
        bufferManager_->ReleasePage(siblingPageId, true);
        bufferManager_->ReleasePage(parentPageId, true);
        return true;
    }

    bool BPlusTree::RedistributeInternalNodes(int underflowPageId, int siblingPageId, int parentPageId, int separatorIdx, bool borrowFromLeft) {
        buffer::Frame* underFrame = bufferManager_->GetPage(underflowPageId);
        BPlusTreeNode underNode(underFrame->page.get());
        buffer::Frame* siblingFrame = bufferManager_->GetPage(siblingPageId);
        BPlusTreeNode siblingNode(siblingFrame->page.get());
        buffer::Frame* parentFrame = bufferManager_->GetPage(parentPageId);
        BPlusTreeNode parentNode(parentFrame->page.get());

        int underNum = underNode.GetNumKeys();
        int sibNum = siblingNode.GetNumKeys();
        if (borrowFromLeft) {
            for (int i = underNum; i > 0; --i) {
                underNode.SetKey(i, underNode.GetKey(i - 1));
                underNode.SetChild(i + 1, maxKeysInternal_, underNode.GetChild(i, maxKeysInternal_));
            }
            underNode.SetChild(0, maxKeysInternal_, siblingNode.GetChild(sibNum, maxKeysInternal_));
            underNode.SetKey(0, parentNode.GetKey(separatorIdx));
            parentNode.SetKey(separatorIdx, siblingNode.GetKey(sibNum - 1));
            siblingNode.SetNumKeys(sibNum - 1);
            underNode.SetNumKeys(underNum + 1);
            int movedChild = underNode.GetChild(0, maxKeysInternal_);
            buffer::Frame* childFrame = bufferManager_->GetPage(movedChild);
            BPlusTreeNode childNode(childFrame->page.get());
            childNode.SetParentPageId(underflowPageId);
            bufferManager_->ReleasePage(movedChild, true);
        } else {
            underNode.SetKey(underNum, parentNode.GetKey(separatorIdx));
            underNode.SetChild(underNum + 1, maxKeysInternal_, siblingNode.GetChild(0, maxKeysInternal_));
            parentNode.SetKey(separatorIdx, siblingNode.GetKey(0));
            for (int i = 0; i < sibNum - 1; ++i) {
                siblingNode.SetKey(i, siblingNode.GetKey(i + 1));
                siblingNode.SetChild(i, maxKeysInternal_, siblingNode.GetChild(i + 1, maxKeysInternal_));
            }
            siblingNode.SetChild(sibNum - 1, maxKeysInternal_, siblingNode.GetChild(sibNum, maxKeysInternal_));
            siblingNode.SetNumKeys(sibNum - 1);
            underNode.SetNumKeys(underNum + 1);
            int movedChild = underNode.GetChild(underNum + 1, maxKeysInternal_);
            buffer::Frame* childFrame = bufferManager_->GetPage(movedChild);
            BPlusTreeNode childNode(childFrame->page.get());
            childNode.SetParentPageId(underflowPageId);
            bufferManager_->ReleasePage(movedChild, true);
        }

        bufferManager_->ReleasePage(underflowPageId, true);
        bufferManager_->ReleasePage(siblingPageId, true);
        bufferManager_->ReleasePage(parentPageId, true);
        return true;
    }

    void BPlusTree::MergeInternalNodes(int leftPageId, int rightPageId, int parentPageId, int separatorIdx) {
        buffer::Frame* leftFrame = bufferManager_->GetPage(leftPageId);
        buffer::Frame* rightFrame = bufferManager_->GetPage(rightPageId);
        buffer::Frame* parentFrame = bufferManager_->GetPage(parentPageId);
        BPlusTreeNode leftNode(leftFrame->page.get());
        BPlusTreeNode rightNode(rightFrame->page.get());
        BPlusTreeNode parentNode(parentFrame->page.get());

        int leftNum = leftNode.GetNumKeys();
        int rightNum = rightNode.GetNumKeys();
        int separatorKey = parentNode.GetKey(separatorIdx);

        std::vector<int32_t> mergedKeys(leftNum + 1 + rightNum);
        std::vector<int32_t> mergedChildren(leftNum + 2 + rightNum);

        for (int i = 0; i < leftNum; ++i) {
            mergedKeys[i] = leftNode.GetKey(i);
        }
        for (int i = 0; i <= leftNum; ++i) {
            mergedChildren[i] = leftNode.GetChild(i, maxKeysInternal_);
        }

        mergedKeys[leftNum] = separatorKey;
        mergedChildren[leftNum + 1] = rightNode.GetChild(0, maxKeysInternal_);

        for (int i = 0; i < rightNum; ++i) {
            mergedKeys[leftNum + 1 + i] = rightNode.GetKey(i);
            mergedChildren[leftNum + 2 + i] = rightNode.GetChild(i + 1, maxKeysInternal_);
        }

        for (int i = 0; i < static_cast<int>(mergedKeys.size()); ++i) {
            leftNode.SetKey(i, mergedKeys[i]);
        }
        for (int i = 0; i < static_cast<int>(mergedChildren.size()); ++i) {
            leftNode.SetChild(i, maxKeysInternal_, mergedChildren[i]);
        }
        leftNode.SetNumKeys(static_cast<int>(mergedKeys.size()));

        for (int i = 0; i <= rightNum; ++i) {
            int childId = rightNode.GetChild(i, maxKeysInternal_);
            if (childId != -1) {
                buffer::Frame* childFrame = bufferManager_->GetPage(childId);
                BPlusTreeNode childNode(childFrame->page.get());
                childNode.SetParentPageId(leftPageId);
                bufferManager_->ReleasePage(childId, true);
            }
        }

        bufferManager_->ReleasePage(leftPageId, true);
        bufferManager_->ReleasePage(rightPageId, true);
        bufferManager_->ReleasePage(parentPageId, false);

        RemoveParentEntry(parentPageId, separatorIdx);
    }

    RID BPlusTree::Search(int key) const {
        if (rootPageId_ == -1) {
            return RID{-1, -1};
        }

        int currentPageId = rootPageId_;
        buffer::Frame* frame = bufferManager_->GetPage(currentPageId);
        BPlusTreeNode node(frame->page.get());

        // Recorrer los nodos internos hasta llegar a un nodo hoja
        while (!node.IsLeaf()) {
            int numKeys = node.GetNumKeys();
            if (numKeys == 0) {
                std::cerr << "Error: Se detecto un nodo interno vacio (posible corrupcion) en pagina " << currentPageId << std::endl;
                break;
            }
            int nextChildIdx = numKeys; // Por defecto el puntero del extremo derecho

            // Buscar la primera clave mayor que la clave buscada
            for (int i = 0; i < numKeys; ++i) {
                if (key < node.GetKey(i)) {
                    nextChildIdx = i;
                    break;
                }
            }

            int nextPageId = node.GetChild(nextChildIdx, maxKeysInternal_);

            // Latch/Pin crabbing: liberar página actual antes de cargar la siguiente
            bufferManager_->ReleasePage(currentPageId, false);
            currentPageId = nextPageId;
            frame = bufferManager_->GetPage(currentPageId);
            node = BPlusTreeNode(frame->page.get());
        }

        // Búsqueda lineal de la clave dentro del nodo hoja
        int numKeys = node.GetNumKeys();
        RID result{-1, -1};
        for (int i = 0; i < numKeys; ++i) {
            if (node.GetKey(i) == key) {
                result = node.GetRID(i, maxKeysLeaf_);
                break;
            }
        }

        bufferManager_->ReleasePage(currentPageId, false);
        return result;
    }

    // CRITICO: Inserción de un registro con división (split) ascendente
    void BPlusTree::Insert(int key, const RID& rid) {
        if (rootPageId_ == -1) {
            rootPageId_ = AllocatePage();
            buffer::Frame* frame = bufferManager_->GetPage(rootPageId_);
            BPlusTreeNode rootNode(frame->page.get());
            rootNode.SetIsLeaf(true);
            rootNode.SetNumKeys(1);
            rootNode.SetParentPageId(-1);
            rootNode.SetNextPageId(-1);
            rootNode.SetPrevPageId(-1);
            rootNode.SetKey(0, key);
            rootNode.SetRID(0, maxKeysLeaf_, rid);
            bufferManager_->ReleasePage(rootPageId_, true);
            PersistRootPageId();
            return;
        }

        int promoKey = -1;
        int promoPageId = -1;
        bool split = InsertRecursive(rootPageId_, key, rid, promoKey, promoPageId);

        // Si la llamada recursiva indica que hubo un split en el nivel de abajo y
        // se propagó hasta la raíz, debemos crear una nueva raíz interna
        if (split) {
            int newRootPageId = AllocatePage();
            buffer::Frame* frame = bufferManager_->GetPage(newRootPageId);
            BPlusTreeNode newRootNode(frame->page.get());
            newRootNode.SetIsLeaf(false);
            newRootNode.SetNumKeys(1);
            newRootNode.SetParentPageId(-1);
            newRootNode.SetKey(0, promoKey);
            newRootNode.SetChild(0, maxKeysInternal_, rootPageId_);
            newRootNode.SetChild(1, maxKeysInternal_, promoPageId);

            // Actualizar la referencia del padre en ambos hijos de la nueva raíz
            buffer::Frame* leftFrame = bufferManager_->GetPage(rootPageId_);
            BPlusTreeNode leftNode(leftFrame->page.get());
            leftNode.SetParentPageId(newRootPageId);
            bufferManager_->ReleasePage(rootPageId_, true);

            buffer::Frame* rightFrame = bufferManager_->GetPage(promoPageId);
            BPlusTreeNode rightNode(rightFrame->page.get());
            rightNode.SetParentPageId(newRootPageId);
            bufferManager_->ReleasePage(promoPageId, true);

            rootPageId_ = newRootPageId;
            bufferManager_->ReleasePage(newRootPageId, true);
            PersistRootPageId();
        }
    }

    // CRITICO: Inserción recursiva interna con manejo de split
    bool BPlusTree::InsertRecursive(int pageId, int key, const RID& rid, int& promoKey, int& promoPageId) {
        buffer::Frame* frame = bufferManager_->GetPage(pageId);
        BPlusTreeNode node(frame->page.get());

        if (node.IsLeaf()) {
            int numKeys = node.GetNumKeys();

            int insertIdx = 0;
            while (insertIdx < numKeys && node.GetKey(insertIdx) < key) {
                insertIdx++;
            }

            if (insertIdx < numKeys && node.GetKey(insertIdx) == key) {
                node.SetRID(insertIdx, maxKeysLeaf_, rid);
                bufferManager_->ReleasePage(pageId, true);
                return false;
            }

            std::vector<int32_t> tempKeys(numKeys + 1);
            std::vector<RID> tempRIDs(numKeys + 1);
            for (int i = 0; i < numKeys; ++i) {
                tempKeys[i] = node.GetKey(i);
                tempRIDs[i] = node.GetRID(i, maxKeysLeaf_);
            }
            for (int i = numKeys; i > insertIdx; --i) {
                tempKeys[i] = tempKeys[i - 1];
                tempRIDs[i] = tempRIDs[i - 1];
            }
            tempKeys[insertIdx] = key;
            tempRIDs[insertIdx] = rid;
            int newNumKeys = numKeys + 1;

            if (newNumKeys <= maxKeysLeaf_) {
                for (int i = 0; i < newNumKeys; ++i) {
                    node.SetKey(i, tempKeys[i]);
                    node.SetRID(i, maxKeysLeaf_, tempRIDs[i]);
                }
                node.SetNumKeys(newNumKeys);
                bufferManager_->ReleasePage(pageId, true);
                return false;
            }

            int splitIdx = newNumKeys / 2;
            int leftCount = splitIdx;
            int rightCount = newNumKeys - splitIdx;
            for (int i = 0; i < leftCount; ++i) {
                node.SetKey(i, tempKeys[i]);
                node.SetRID(i, maxKeysLeaf_, tempRIDs[i]);
            }
            node.SetNumKeys(leftCount);

            int newPageId = AllocatePage();
            buffer::Frame* newFrame = bufferManager_->GetPage(newPageId);
            BPlusTreeNode newNode(newFrame->page.get());
            newNode.SetIsLeaf(true);
            newNode.SetParentPageId(node.GetParentPageId());
            for (int i = 0; i < rightCount; ++i) {
                newNode.SetKey(i, tempKeys[leftCount + i]);
                newNode.SetRID(i, maxKeysLeaf_, tempRIDs[leftCount + i]);
            }
            newNode.SetNumKeys(rightCount);
            newNode.SetNextPageId(node.GetNextPageId());
            newNode.SetPrevPageId(pageId);
            node.SetNextPageId(newPageId);
            if (newNode.GetNextPageId() != -1) {
                buffer::Frame* nextFrame = bufferManager_->GetPage(newNode.GetNextPageId());
                BPlusTreeNode nextNode(nextFrame->page.get());
                nextNode.SetPrevPageId(newPageId);
                bufferManager_->ReleasePage(newNode.GetNextPageId(), true);
            }
            promoKey = newNode.GetKey(0);
            promoPageId = newPageId;
            bufferManager_->ReleasePage(pageId, true);
            bufferManager_->ReleasePage(newPageId, true);
            return true;
        } else {
            // ---------------------------------------------------------------
            // Nodo Interno: determinar a qué hijo descender
            // ---------------------------------------------------------------
            int numKeys = node.GetNumKeys();
            if (numKeys == 0) {
                std::cerr << "Error: Se detecto un nodo interno vacio (posible corrupcion) durante la insercion en pagina " << pageId << std::endl;
                bufferManager_->ReleasePage(pageId, false);
                return false;
            }
            int childIdx = numKeys;
            for (int i = 0; i < numKeys; ++i) {
                if (key < node.GetKey(i)) {
                    childIdx = i;
                    break;
                }
            }

            int childPageId = node.GetChild(childIdx, maxKeysInternal_);

            // CRITICO: Mantener el padre pinned evita recargas con datos desactualizados durante la recursión.
            bufferManager_->ReleasePage(pageId, false);

            int childPromoKey = -1;
            int childPromoPageId = -1;
            bool split = InsertRecursive(childPageId, key, rid, childPromoKey, childPromoPageId);

            if (split) {
                frame = bufferManager_->GetPage(pageId);
                node = BPlusTreeNode(frame->page.get());
                numKeys = node.GetNumKeys();

                if (!node.IsLeaf() && numKeys == 0 && node.GetParentPageId() == 0 && node.GetKey(0) == 0) {
                    std::cerr << "Advertencia: La recarga del nodo padre " << pageId << " produjo datos inesperados." << std::endl;
                }

                int insertIdx = 0;
                while (insertIdx < numKeys && node.GetKey(insertIdx) < childPromoKey) {
                    insertIdx++;
                }
                std::vector<int32_t> tempKeys(numKeys + 1);
                std::vector<int32_t> tempChildren(numKeys + 2);
                for (int i = 0; i < numKeys; ++i) {
                    tempKeys[i] = node.GetKey(i);
                }
                for (int i = 0; i <= numKeys; ++i) {
                    tempChildren[i] = node.GetChild(i, maxKeysInternal_);
                }
                for (int i = numKeys; i > insertIdx; --i) {
                    tempKeys[i] = tempKeys[i - 1];
                }
                for (int i = numKeys + 1; i > insertIdx + 1; --i) {
                    tempChildren[i] = tempChildren[i - 1];
                }
                tempKeys[insertIdx] = childPromoKey;
                tempChildren[insertIdx + 1] = childPromoPageId;
                int newNumKeys = numKeys + 1;
                if (newNumKeys <= maxKeysInternal_) {
                    for (int i = 0; i < newNumKeys; ++i) {
                        node.SetKey(i, tempKeys[i]);
                    }
                    for (int i = 0; i <= newNumKeys; ++i) {
                        node.SetChild(i, maxKeysInternal_, tempChildren[i]);
                    }
                    node.SetNumKeys(newNumKeys);
                    bufferManager_->ReleasePage(pageId, true);
                    return false;
                }


                // Con split: dividir el nodo interno
                int newPageId = AllocatePage();
                buffer::Frame* newFrame = bufferManager_->GetPage(newPageId);
                BPlusTreeNode newNode(newFrame->page.get());
                newNode.SetIsLeaf(false);
                newNode.SetParentPageId(node.GetParentPageId());

                int splitIdx = newNumKeys / 2;
                promoKey = tempKeys[splitIdx]; // Esta clave sube al padre

                int leftCount = splitIdx;
                int rightCount = newNumKeys - splitIdx - 1;
                for (int i = 0; i < leftCount; ++i) {
                    node.SetKey(i, tempKeys[i]);
                }
                for (int i = 0; i <= leftCount; ++i) {
                    node.SetChild(i, maxKeysInternal_, tempChildren[i]);
                }
                node.SetNumKeys(leftCount);

                newNode.SetChild(0, maxKeysInternal_, tempChildren[splitIdx + 1]);
                for (int i = 0; i < rightCount; ++i) {
                    newNode.SetKey(i, tempKeys[splitIdx + 1 + i]);
                    newNode.SetChild(i + 1, maxKeysInternal_, tempChildren[splitIdx + 2 + i]);
                }
                newNode.SetNumKeys(rightCount);

                // Actualizar el ParentPageId de los hijos movidos al nuevo nodo
                for (int i = 0; i <= rightCount; ++i) {
                    int childId = newNode.GetChild(i, maxKeysInternal_);
                    buffer::Frame* childFrame = bufferManager_->GetPage(childId);
                    BPlusTreeNode childNode(childFrame->page.get());
                    childNode.SetParentPageId(newPageId);
                    bufferManager_->ReleasePage(childId, true);
                }

                promoPageId = newPageId;

                bufferManager_->ReleasePage(pageId, true);
                bufferManager_->ReleasePage(newPageId, true);
                return true;
            }

            return false;
        }
    }

    void BPlusTree::PrintTree() {
        std::cout << "\n========== ESTRUCTURA DEL B+ TREE ==========" << std::endl;
        if (rootPageId_ == -1) {
            std::cout << "(Arbol vacio)" << std::endl;
        } else {
            PrintTreeInternal(rootPageId_, "");
        }
        std::cout << "============================================\n" << std::endl;
    }

    void BPlusTree::PrintTreeInternal(int pageId, std::string indent) {
        buffer::Frame* frame = bufferManager_->GetPage(pageId);
        BPlusTreeNode node(frame->page.get());

        int numKeys = node.GetNumKeys();
        if (node.IsLeaf()) {
            std::cout << indent << "[Pagina Hoja " << pageId << "] Claves: ";
            std::cout << "[";
            for (int i = 0; i < numKeys; ++i) {
                std::cout << node.GetKey(i);
                if (i < numKeys - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;
        } else {
            std::cout << indent << "[Pagina Interna " << pageId << "] Claves: ";
            std::cout << "[";
            for (int i = 0; i < numKeys; ++i) {
                std::cout << node.GetKey(i);
                if (i < numKeys - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;

            // Recorrer los hijos identándolos
            if (numKeys == 0) {
                std::cout << indent << "    [Vacio - Posible Corrupcion]" << std::endl;
            } else {
                for (int i = 0; i <= numKeys; ++i) {
                    int childId = node.GetChild(i, maxKeysInternal_);
                    PrintTreeInternal(childId, indent + "    ");
                }
            }
        }

        bufferManager_->ReleasePage(pageId, false);
    }
}
