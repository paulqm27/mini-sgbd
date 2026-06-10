#include "storage.h"
#include <iostream>

namespace storage {

    StorageManager::StorageManager(const std::string& filename) : filename_(filename), numPages_(0) {
        file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) {
            file_.clear();
            file_.open(filename, std::ios::out | std::ios::binary);
            file_.close();
            file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        }
        // Calcular numPages_ una única vez desde el tamaño real del archivo en disco.
        // NO se actualiza en ReadPage: solo las escrituras determinan cuántas páginas existen.
        if (file_.is_open()) {
            file_.clear();
            file_.seekg(0, std::ios::end);
            std::streampos size = file_.tellg();
            file_.clear();
            numPages_ = static_cast<int>(size / PAGE_SIZE);
        }
    }

    StorageManager::~StorageManager() {
        Close();
    }

    bool StorageManager::WritePage(int pageId, const std::vector<uint8_t>& data) {
        if (!file_.is_open()) return false;

        // Solo al escribir actualizamos numPages_: esto garantiza que AllocatePage
        // siempre devuelva un ID correcto (igual al número de páginas físicas reales).
        if (pageId >= numPages_) {
            numPages_ = pageId + 1;
        }

        file_.clear();
        std::streampos pos = static_cast<std::streampos>(static_cast<int64_t>(pageId) * PAGE_SIZE);
        file_.seekp(pos);
        if (!file_.good()) {
            file_.clear();
            file_.seekp(pos);
        }
        file_.write(reinterpret_cast<const char*>(data.data()), PAGE_SIZE);
        file_.flush();
        bool ok = file_.good();
        file_.clear();
        return ok;
    }

    std::vector<uint8_t> StorageManager::ReadPage(int pageId) {
        std::vector<uint8_t> data(PAGE_SIZE, 0);
        if (!file_.is_open()) return data;

        // NOTA: ReadPage NO actualiza numPages_ para no confundir la contabilidad de páginas.
        file_.clear();
        std::streampos pos = static_cast<std::streampos>(static_cast<int64_t>(pageId) * PAGE_SIZE);
        file_.seekg(pos);
        if (!file_.good()) {
            file_.clear();
            return data; // Página no existe aún en disco: devolver ceros
        }
        file_.read(reinterpret_cast<char*>(data.data()), PAGE_SIZE);
        file_.clear();
        return data;
    }

    bool StorageManager::WritePageData(int pageId, const Page& page) {
        return WritePage(pageId, page.GetData());
    }

    std::unique_ptr<Page> StorageManager::ReadPageData(int pageId) {
        std::vector<uint8_t> data = ReadPage(pageId);
        return std::make_unique<Page>(data);
    }

    void StorageManager::Close() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    int StorageManager::GetNumPages() {
        return numPages_;
    }

}
