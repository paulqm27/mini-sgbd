#pragma once

#include "storage/storage.h"
#include "storage/page.h"
#include "buffer/buffer.h"
#include <cstdint>
#include <string>
#include <vector>

namespace index_m {

    // Estructura RID (Record Identifier) para asociar claves con registros físicos
    struct RID {
        int32_t pageId = -1;
        int32_t slotId = -1;

        bool IsValid() const {
            return pageId != -1 && slotId != -1;
        }
    };

    // Clase para interactuar con la representación binaria de un nodo B+ Tree en una página de 4KB
    class BPlusTreeNode {
    public:
        explicit BPlusTreeNode(storage::Page* page) : page_(page) {}

        // Métodos de acceso al encabezado (Header)
        bool IsLeaf() const;
        void SetIsLeaf(bool is_leaf);

        uint16_t GetNumKeys() const;
        void SetNumKeys(uint16_t num_keys);

        int32_t GetParentPageId() const;
        void SetParentPageId(int32_t parent_page_id);

        int32_t GetNextPageId() const;
        void SetNextPageId(int32_t next_page_id);

        int32_t GetPrevPageId() const;
        void SetPrevPageId(int32_t prev_page_id);

        // Métodos de acceso a claves (Keys)
        int32_t GetKey(int idx) const;
        void SetKey(int idx, int32_t key);

        // Métodos de acceso a valores (RIDs para hojas)
        RID GetRID(int idx, int max_keys) const;
        void SetRID(int idx, int max_keys, const RID& rid);

        // Métodos de acceso a punteros a hijos (Page IDs para nodos internos)
        int32_t GetChild(int idx, int max_keys) const;
        void SetChild(int idx, int max_keys, int32_t child_page_id);

    private:
        storage::Page* page_;
    };

    // Estructura raíz y lógica del árbol B+ Tree integrada con el Buffer Manager
    class BPlusTree {
    public:
        static constexpr int METADATA_PAGE_ID = 0;

        BPlusTree(buffer::BufferManager* bufferManager, storage::StorageManager* storageManager, int maxKeysLeaf = 3, int maxKeysInternal = 3);
        
        // Búsqueda exacta de un registro por clave
        RID Search(int key) const;

        // Inserción de un par clave-RID con split automático
        void Insert(int key, const RID& rid);

        // Eliminación de una clave y reequilibrio del árbol
        bool Delete(int key);

        // Imprime la estructura completa del árbol de forma indentada
        void PrintTree();

        // Retorna el ID de la página raíz
        int GetRootPageId() const { return rootPageId_; }

    private:
        void LoadRootPageId();
        void PersistRootPageId();
        int32_t ReadMetadataRootPageId();
        void WriteMetadataRootPageId(int32_t rootPageId);
        int GetMinKeys(bool is_leaf) const;
        int FindChildIndex(BPlusTreeNode& parent, int childPageId) const;
        bool DeleteEntryFromLeaf(int pageId, int key);
        bool HandleUnderflow(int pageId);
        bool RedistributeLeafNodes(int underflowPageId, int siblingPageId, int parentPageId, int separatorIdx, bool borrowFromLeft);
        bool RedistributeInternalNodes(int underflowPageId, int siblingPageId, int parentPageId, int separatorIdx, bool borrowFromLeft);
        void RemoveParentEntry(int parentPageId, int separatorIdx);
        void MergeLeafNodes(int leftPageId, int rightPageId, int parentPageId, int separatorIdx);
        void MergeInternalNodes(int leftPageId, int rightPageId, int parentPageId, int separatorIdx);

        struct PathEntry {
            int parentPageId;
            int childIndex;
        };

        buffer::BufferManager* bufferManager_;
        storage::StorageManager* storageManager_;
        int rootPageId_;
        int maxKeysLeaf_;
        int maxKeysInternal_;

        // Reserva una nueva página vacía en disco para un nodo
        int AllocatePage();

        // Función recursiva interna para la inserción
        bool InsertRecursive(int pageId, int key, const RID& rid, int& promoKey, int& promoPageId);

        // Imprime jerárquicamente a partir de un nodo específico
        void PrintTreeInternal(int pageId, std::string indent);
    };

}
