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
    RID BPlusTree::Search(int key) {
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
            // Crear la primera página raíz del árbol B+ (que comienza siendo hoja)
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
        }
    }

    // CRITICO: Inserción recursiva interna con manejo de split
    bool BPlusTree::InsertRecursive(int pageId, int key, const RID& rid, int& promoKey, int& promoPageId) {
        buffer::Frame* frame = bufferManager_->GetPage(pageId);
        BPlusTreeNode node(frame->page.get());

        if (node.IsLeaf()) {
            int numKeys = node.GetNumKeys();

            // Encontrar la posición de inserción manteniendo el orden
            int insertIdx = 0;
            while (insertIdx < numKeys && node.GetKey(insertIdx) < key) {
                insertIdx++;
            }

            // Clave duplicada: actualizar el RID
            if (insertIdx < numKeys && node.GetKey(insertIdx) == key) {
                node.SetRID(insertIdx, maxKeysLeaf_, rid);
                bufferManager_->ReleasePage(pageId, true);
                return false;
            }

            // Desplazar elementos hacia la derecha para hacer espacio
            for (int i = numKeys; i > insertIdx; --i) {
                node.SetKey(i, node.GetKey(i - 1));
                node.SetRID(i, maxKeysLeaf_, node.GetRID(i - 1, maxKeysLeaf_));
            }

            // Insertar nueva clave y valor
            node.SetKey(insertIdx, key);
            node.SetRID(insertIdx, maxKeysLeaf_, rid);
            numKeys++;
            node.SetNumKeys(numKeys);

            // Si no se supera el límite de llaves, guardamos y salimos
            if (numKeys <= maxKeysLeaf_) {
                bufferManager_->ReleasePage(pageId, true);
                return false;
            }

            // Superó la capacidad: dividir el nodo hoja
            int newPageId = AllocatePage();
            buffer::Frame* newFrame = bufferManager_->GetPage(newPageId);
            BPlusTreeNode newNode(newFrame->page.get());
            newNode.SetIsLeaf(true);
            newNode.SetParentPageId(node.GetParentPageId());

            // Dividir las claves a la mitad
            int splitIdx = numKeys / 2;
            int moveCount = numKeys - splitIdx;

            for (int i = 0; i < moveCount; ++i) {
                newNode.SetKey(i, node.GetKey(splitIdx + i));
                newNode.SetRID(i, maxKeysLeaf_, node.GetRID(splitIdx + i, maxKeysLeaf_));
            }
            newNode.SetNumKeys(moveCount);
            node.SetNumKeys(splitIdx);

            // Reorganizar los punteros secuenciales de los nodos hojas (siguiente y anterior)
            newNode.SetNextPageId(node.GetNextPageId());
            newNode.SetPrevPageId(pageId);
            node.SetNextPageId(newPageId);

            if (newNode.GetNextPageId() != -1) {
                buffer::Frame* nextFrame = bufferManager_->GetPage(newNode.GetNextPageId());
                BPlusTreeNode nextNode(nextFrame->page.get());
                nextNode.SetPrevPageId(newPageId);
                bufferManager_->ReleasePage(newNode.GetNextPageId(), true);
            }

            // Promover la primera clave del nuevo nodo hoja al nodo padre
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

            // Si el nodo hijo se dividió, debemos insertar la clave promovida en este nodo interno
            if (split) {
                frame = bufferManager_->GetPage(pageId);
                node = BPlusTreeNode(frame->page.get());
                numKeys = node.GetNumKeys();

                // Verificar que la página recargada es válida
                if (!node.IsLeaf() && numKeys == 0 && node.GetParentPageId() == 0 && node.GetKey(0) == 0) {
                    // La página se recargó con datos incorrectos; esto no debería ocurrir
                    // con la corrección de numPages_, pero se deja como salvaguarda.
                    std::cerr << "Advertencia: La recarga del nodo padre " << pageId << " produjo datos inesperados." << std::endl;
                }

                int insertIdx = 0;
                while (insertIdx < numKeys && node.GetKey(insertIdx) < childPromoKey) {
                    insertIdx++;
                }

                // Desplazar claves y punteros hacia la derecha
                for (int i = numKeys; i > insertIdx; --i) {
                    node.SetKey(i, node.GetKey(i - 1));
                }
                for (int i = numKeys + 1; i > insertIdx + 1; --i) {
                    node.SetChild(i, maxKeysInternal_, node.GetChild(i - 1, maxKeysInternal_));
                }

                // Insertar clave promovida y puntero al nuevo nodo hermano
                node.SetKey(insertIdx, childPromoKey);
                node.SetChild(insertIdx + 1, maxKeysInternal_, childPromoPageId);
                numKeys++;
                node.SetNumKeys(numKeys);

                // Sin split: guardar y terminar
                if (numKeys <= maxKeysInternal_) {
                    bufferManager_->ReleasePage(pageId, true);
                    return false;
                }

                // Con split: dividir el nodo interno
                int newPageId = AllocatePage();
                buffer::Frame* newFrame = bufferManager_->GetPage(newPageId);
                BPlusTreeNode newNode(newFrame->page.get());
                newNode.SetIsLeaf(false);
                newNode.SetParentPageId(node.GetParentPageId());

                int splitIdx = numKeys / 2;
                promoKey = node.GetKey(splitIdx); // Esta clave sube al padre

                // Mover la mitad derecha al nuevo nodo interno
                int moveCount = numKeys - splitIdx - 1;
                newNode.SetChild(0, maxKeysInternal_, node.GetChild(splitIdx + 1, maxKeysInternal_));
                for (int i = 0; i < moveCount; ++i) {
                    newNode.SetKey(i, node.GetKey(splitIdx + 1 + i));
                    newNode.SetChild(i + 1, maxKeysInternal_, node.GetChild(splitIdx + 2 + i, maxKeysInternal_));
                }
                newNode.SetNumKeys(moveCount);
                node.SetNumKeys(splitIdx);

                // Actualizar el ParentPageId de los hijos movidos al nuevo nodo
                for (int i = 0; i <= moveCount; ++i) {
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
