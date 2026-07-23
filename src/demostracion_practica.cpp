#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "storage/storage.h"
#include "buffer/buffer.h"
#include "index/bplus_tree.h"
#include "execution/scan.h"
#include "execution/select.h"
#include "execution/project.h"
#include "execution/nested_loop_join.h"
#include "execution/index_scan.h"
#include "execution/planner.h"

using namespace storage;
using namespace buffer;
using namespace index_m;
using namespace exec;

namespace {

std::vector<uint8_t> ToBytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::string ToText(const std::vector<uint8_t>& bytes) {
    return {bytes.begin(), bytes.end()};
}

int ParseId(const std::vector<uint8_t>& record) {
    const std::string text = ToText(record);
    const std::size_t pos = text.find("ID:");
    if (pos == std::string::npos) return -1;
    try {
        return std::stoi(text.substr(pos + 3));
    } catch (...) {
        return -1;
    }
}

std::string ProjectStudent(const std::vector<uint8_t>& record) {
    const std::string text = ToText(record);
    const std::size_t idPos = text.find("ID:");
    if (idPos == std::string::npos) return text;
    return "PROYECCION -> " + text.substr(0, idPos) + text.substr(idPos);
}

RID InsertRecordWithDebug(BufferManager& bm, int pageId, const std::string& record) {
    std::cout << "\n[STORAGE + BUFFER] Insertando: \"" << record << "\"\n";
    Frame* frame = bm.GetPage(pageId);
    std::cout << "  GetPage(" << pageId << ") -> frame cargado; pinCount="
              << frame->pinCount << ", dirty=" << (frame->dirty ? "SI" : "NO") << "\n";

    if (!frame->page->InsertRecord(ToBytes(record))) {
        std::cout << "  La pagina " << pageId
                  << " no tiene espacio: se libera y se debe usar una pagina nueva.\n";
        bm.ReleasePage(pageId, false);
        return RID{-1, -1};
    }

    const int slotId = frame->page->GetNumSlots() - 1;
    std::cout << "  Page::InsertRecord creo el slot " << slotId
              << "; slots actuales=" << frame->page->GetNumSlots() << "\n";
    bm.ReleasePage(pageId, true);
    std::cout << "  ReleasePage(" << pageId
              << ", dirty=true) -> el cambio queda en el Buffer Pool hasta Flush().\n";
    return RID{pageId, slotId};
}

void PrintBanner(const std::string& title) {
    std::cout << "\n================================================================\n";
    std::cout << title << "\n";
    std::cout << "================================================================\n";
}

}  // namespace

int main() {
    const std::string databaseFile = "data/demostracion_practica.db";
    std::remove(databaseFile.c_str());  // La demo es reproducible: inicia con una base vacía.

    PrintBanner("DEMO PRACTICA: MINI SGBD INTEGRADO");
    std::cout << "Archivo binario de la demo: " << databaseFile << "\n";
    std::cout << "Tamano fijo de pagina: " << PAGE_SIZE << " bytes\n";
    std::cout << "Pagina 0: metadatos del B+ Tree; paginas 1 y 2: tablas de la demo.\n";

    {
        StorageManager storageManager(databaseFile);
        BufferManager bufferManager(20, &storageManager);

        PrintBanner("1. CARGA DE DATOS: SLOT DIRECTORY + PAGINAS DIRTY");
        std::cout << "Cada registro se guarda en una pagina binaria y recibe un RID = (pagina, slot).\n";

        const int studentsPage = 1;
        const int enrollmentsPage = 2;
        const std::vector<std::string> students = {
            "ALUMNO: Ana, ID:1, Carrera: Sistemas",
            "ALUMNO: Bruno, ID:2, Carrera: Sistemas",
            "ALUMNO: Carla, ID:3, Carrera: Industrial",
            "ALUMNO: Diego, ID:4, Carrera: Sistemas",
            "ALUMNO: Elena, ID:5, Carrera: Industrial",
            "ALUMNO: Fabio, ID:6, Carrera: Sistemas"
        };
        const std::vector<std::string> enrollments = {
            "MATRICULA: BD-I, ID:3, Ciclo:2026-1",
            "MATRICULA: BD-I, ID:4, Ciclo:2026-1",
            "MATRICULA: BD-I, ID:5, Ciclo:2026-1",
            "MATRICULA: BD-I, ID:6, Ciclo:2026-1",
            "MATRICULA: BD-I, ID:7, Ciclo:2026-1",
            "MATRICULA: BD-I, ID:8, Ciclo:2026-1"
        };

        for (const auto& student : students) {
            const RID rid = InsertRecordWithDebug(bufferManager, studentsPage, student);
            std::cout << "  RID asignado al alumno: (" << rid.pageId << ", " << rid.slotId << ")\n";
        }
        for (const auto& enrollment : enrollments) {
            const RID rid = InsertRecordWithDebug(bufferManager, enrollmentsPage, enrollment);
            std::cout << "  RID asignado a la matricula: (" << rid.pageId << ", " << rid.slotId << ")\n";
        }

        std::cout << "\n[BUFFER] Estado antes de Flush: las paginas de datos estan dirty.\n";
        bufferManager.PrintStatus();
        std::cout << "[STORAGE] Flush(): se escriben las paginas dirty al archivo binario.\n";
        bufferManager.Flush();
        std::cout << "[STORAGE] Paginas fisicas en disco: " << storageManager.GetNumPages() << "\n";

        PrintBanner("2. INDICE PRINCIPAL: B+ TREE PERSISTENTE");
        std::cout << "Se indexa Matricula.ID. Cada entrada del arbol sera clave -> RID.\n";
        BPlusTree index(&bufferManager, &storageManager, 3, 3);
        Frame* enrollmentFrame = bufferManager.GetPage(enrollmentsPage);
        const int enrollmentSlots = enrollmentFrame->page->GetNumSlots();
        std::cout << "[BUFFER] GetPage(" << enrollmentsPage << ") para leer "
                  << enrollmentSlots << " slots de Matricula.\n";
        for (int slot = 0; slot < enrollmentSlots; ++slot) {
            const std::vector<uint8_t> record = enrollmentFrame->page->ReadRecord(slot);
            const int key = ParseId(record);
            const RID rid{enrollmentsPage, slot};
            std::cout << "  BPlusTree::Insert(clave=" << key << ", RID=("
                      << rid.pageId << ", " << rid.slotId << "))\n";
            index.Insert(key, rid);
        }
        bufferManager.ReleasePage(enrollmentsPage, false);
        bufferManager.Flush();
        std::cout << "[INDEX] Root page id persistido: " << index.GetRootPageId() << "\n";
        std::cout << "[INDEX] El orden pequeno (3 claves) fuerza splits visibles:\n";
        index.PrintTree();

        PrintBanner("3. PLAN VOLCANO: Scan -> Select -> Project");
        std::cout << "Consulta conceptual: alumnos cuyo ID es par, mostrando la proyeccion solicitada.\n";
        Scan studentsScan(&bufferManager, studentsPage, studentsPage);
        Select evenId(&studentsScan, [](const std::vector<uint8_t>& record) {
            return ParseId(record) % 2 == 0;
        });
        Project projection(&evenId, [](const std::vector<uint8_t>& record) {
            return ToBytes(ProjectStudent(record));
        });

        std::cout << "[VOLCANO] open() se propaga: Project -> Select -> Scan.\n";
        projection.open();
        std::vector<uint8_t> output;
        while (projection.next(output)) {
            std::cout << "  next() produjo: " << ToText(output) << "\n";
        }
        projection.close();
        std::cout << "[VOLCANO] close() libera el frame que Scan pudiera mantener pinned.\n";

        PrintBanner("4. PLANIFICADOR + INDEX SCAN");
        PredicateInfo predicate{true, 5};
        std::cout << "Predicado: Matricula.ID = " << predicate.key << "\n";
        if (Planner::UseIndexScan(&index, predicate)) {
            std::cout << "[PLANNER] Hay indice y el predicado es igualdad: elige IndexScan.\n";
            IndexScan indexScan(&index, &bufferManager, predicate.key);
            indexScan.open();
            if (indexScan.next(output)) {
                std::cout << "[INDEX SCAN] B+ Tree encontro el RID y luego leyo la pagina de datos:\n";
                std::cout << "  resultado = " << ToText(output) << "\n";
            }
            indexScan.close();
        } else {
            std::cout << "[PLANNER] Se habria elegido Scan secuencial.\n";
        }

        PrintBanner("5. NESTED LOOP JOIN: Alumno x Matricula");
        std::cout << "Condicion: Alumno.ID = Matricula.ID. El inner se materializa y se compara con cada outer.\n";
        Scan leftScan(&bufferManager, studentsPage, studentsPage);
        Scan rightScan(&bufferManager, enrollmentsPage, enrollmentsPage);
        NestedLoopJoin join(&leftScan, &rightScan,
            [](const std::vector<uint8_t>& left, const std::vector<uint8_t>& right) {
                return ParseId(left) == ParseId(right);
            });
        join.open();
        int joinResults = 0;
        while (join.next(output)) {
            ++joinResults;
            std::cout << "  join.next() -> " << ToText(output) << "\n";
        }
        join.close();
        std::cout << "[JOIN] Total de tuplas resultantes: " << joinResults << "\n";

        PrintBanner("6. BUFFER POOL: LRU, PINNED Y METRICAS");
        std::cout << "Se usa un pool auxiliar de 3 frames para forzar reemplazos LRU sin alterar la consulta.\n";
        BufferManager lruDemo(3, &storageManager);
        const int totalPages = storageManager.GetNumPages();
        for (int pageId = 0; pageId < totalPages; ++pageId) {
            Frame* frame = lruDemo.GetPage(pageId);
            std::cout << "  GetPage(" << pageId << ") -> pinCount=" << frame->pinCount
                      << "; ReleasePage(..., dirty=false)\n";
            lruDemo.ReleasePage(pageId, false);
        }
        std::cout << "[LRU] Despues de acceder mas paginas que frames, las menos recientes fueron expulsadas.\n";
        lruDemo.PrintStatus();
        Frame* pinned = lruDemo.GetPage(0);
        std::cout << "[PIN] Pagina 0 queda fijada temporalmente: pinCount=" << pinned->pinCount << "\n";
        lruDemo.ReleasePage(0, false);
        std::cout << "[PIN] Pagina 0 liberada: vuelve a ser candidata LRU.\n";

        std::cout << "\n[METRICAS] Buffer principal: accesos=" << bufferManager.GetAccessCount()
                  << ", hits=" << bufferManager.GetHitCount()
                  << ", misses=" << bufferManager.GetMissCount()
                  << ", hit rate=" << bufferManager.GetHitRate() * 100.0 << "%\n";
        bufferManager.Flush();
        std::cout << "[FIN DE PRIMERA SESION] Todos los cambios fueron sincronizados.\n";
    }

    PrintBanner("7. VERIFICACION DE PERSISTENCIA: REABRIR EL ARCHIVO");
    StorageManager reloadedStorage(databaseFile);
    BufferManager reloadedBuffer(10, &reloadedStorage);
    BPlusTree reloadedIndex(&reloadedBuffer, &reloadedStorage, 3, 3);
    std::cout << "[RELOAD] Archivo abierto otra vez. Root page id recuperado desde pagina 0: "
              << reloadedIndex.GetRootPageId() << "\n";
    const RID found = reloadedIndex.Search(5);
    if (found.IsValid()) {
        Frame* dataFrame = reloadedBuffer.GetPage(found.pageId);
        const std::vector<uint8_t> record = dataFrame->page->ReadRecord(found.slotId);
        reloadedBuffer.ReleasePage(found.pageId, false);
        std::cout << "[RELOAD] Search(5) -> RID=(" << found.pageId << ", " << found.slotId
                  << ") -> \"" << ToText(record) << "\"\n";
    } else {
        std::cout << "[RELOAD] ERROR: no se encontro la clave 5.\n";
        return 1;
    }

    PrintBanner("DEMO FINALIZADA");
    std::cout << "Se ejercitaron conjuntamente: Page/Slot Directory, Storage, Buffer/LRU,\n"
              << "B+ Tree persistente, Volcano, Select, Project, IndexScan y NestedLoopJoin.\n";
    return 0;
}
