#include <iostream>
#include <string>
#include <vector>

#include "storage/storage.h"
#include "buffer/buffer.h"

int main() {

    // ========================================
    // SEMANA 2 & 3
    // Persistencia basica en archivos binarios
    // ========================================

    std::cout << "===== SEMANA 2 & 3 =====" << std::endl;
    std::cout << "Persistencia basica en disco" << std::endl;

    storage::StorageManager gestorStorage("../data/database.db");

    storage::Page pagina;

    std::string r1 = "Juan";
    std::string r2 = "Pedro";
    std::string r3 = "Maria";

    pagina.InsertRecord(std::vector<uint8_t>(r1.begin(), r1.end()));
    pagina.InsertRecord(std::vector<uint8_t>(r2.begin(), r2.end()));
    pagina.InsertRecord(std::vector<uint8_t>(r3.begin(), r3.end()));

    if (!gestorStorage.WritePageData(0, pagina)) {

        std::cerr << "Error al escribir la pagina en disco" << std::endl;
        return 1;
    }

    std::cout << "Pagina guardada correctamente en disco" << std::endl;

    auto paginaLeida = gestorStorage.ReadPageData(0);

    if (!paginaLeida) {

        std::cerr << "Error al leer la pagina desde disco" << std::endl;
        return 1;
    }

    std::cout << "\nRegistros recuperados desde disco:" << std::endl;

    for (const auto& registro : paginaLeida->ReadAllRecords()) {

        std::string s(registro.begin(), registro.end());

        std::cout << "- " << s << std::endl;
    }

    // ========================================
    // SEMANA 6
    // Buffer Manager + Politica LRU
    // ========================================

    std::cout << "\n===== SEMANA 6 =====" << std::endl;
    std::cout << "Buffer Manager con politica LRU" << std::endl;

    buffer::BufferManager bufferManager(2, &gestorStorage);

    // ========================================
    // PAGINA 0
    // ========================================

    auto frame1 = bufferManager.GetPage(0);

    std::string sr1 = "S6 - Registro A";
    std::string sr2 = "S6 - Registro B";

    frame1->page->InsertRecord(std::vector<uint8_t>(sr1.begin(), sr1.end()));
    frame1->page->InsertRecord(std::vector<uint8_t>(sr2.begin(), sr2.end()));

    bufferManager.ReleasePage(0, true);

    std::cout << "Pagina 0 cargada en buffer" << std::endl;

    // ========================================
    // PAGINA 1
    // ========================================

    auto frame2 = bufferManager.GetPage(1);

    std::string sr3 = "Pagina 1 - Dato X";

    frame2->page->InsertRecord(std::vector<uint8_t>(sr3.begin(), sr3.end()));

    bufferManager.ReleasePage(1, true);

    std::cout << "Pagina 1 cargada en buffer" << std::endl;

    // ========================================
    // PAGINA 2
    // Fuerza politica LRU
    // ========================================

    auto frame3 = bufferManager.GetPage(2);

    std::string sr4 = "Pagina 2 - LRU";

    frame3->page->InsertRecord(std::vector<uint8_t>(sr4.begin(), sr4.end()));

    bufferManager.ReleasePage(2, true);

    std::cout << "Pagina 2 cargada" << std::endl;
    std::cout << "Politica LRU ejecutada" << std::endl;

    // Flush manual
    bufferManager.Flush();

    std::cout << "Buffer Manager finalizado" << std::endl;

    // ========================================
    // VERIFICACION FINAL
    // ========================================

    std::cout << "\n===== VERIFICACION =====" << std::endl;

    for (int i = 0; i < 3; i++) {

        auto p = gestorStorage.ReadPageData(i);

        std::cout << "\nPagina " << i << std::endl;

        if (p) {

            auto registros = p->ReadAllRecords();

            if (registros.empty()) {

                std::cout << "(Sin registros)" << std::endl;
            }

            for (const auto& registro : registros) {

                std::string s(registro.begin(), registro.end());

                std::cout << "- " << s << std::endl;
            }
        }
    }

    std::cout << "\nEjecucion finalizada correctamente." << std::endl;

    return 0;
}
