#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <memory>

#include "storage/storage.h"
#include "buffer/buffer.h"
#include "index/bplus_tree.h"

using namespace std;
using namespace storage;
using namespace buffer;
using namespace index_m;

// Helper struct for table records insertion
struct TableRID {
    int32_t pageId;
    int32_t slotId;
};

// Función para insertar registros de datos usando el Buffer Manager
TableRID InsertRecordToTable(BufferManager& bm, const string& recordStr, int& currentDataPageId) {
    vector<uint8_t> record(recordStr.begin(), recordStr.end());
    Frame* frame = bm.GetPage(currentDataPageId);

    // Si la página actual no tiene espacio, pasamos a la siguiente
    if (!frame->page->InsertRecord(record)) {
        bm.ReleasePage(currentDataPageId, false); // Liberar sin marcar dirty ya que falló
        currentDataPageId++;
        frame = bm.GetPage(currentDataPageId);
        frame->page->InsertRecord(record);
    }

    TableRID rid;
    rid.pageId = currentDataPageId;
    rid.slotId = frame->page->GetNumSlots() - 1;

    // Liberamos marcando dirty para que se guarde en disco
    bm.ReleasePage(currentDataPageId, true);
    return rid;
}

int main() {
    auto start_time = chrono::high_resolution_clock::now();

    cout << "=====================================================================" << endl;
    cout << "          DEMOSTRACION COMPLETA DEL MINI SGBD - SEMANA 10            " << endl;
    cout << "=====================================================================" << endl;

    // =====================================================================
    // A. Inicialización
    // =====================================================================
    cout << "\n[A] INICIALIZANDO COMPONENTES..." << endl;

        string dbFilename = "data/database.db";
    // Limpiar base de datos previa para la demo 
    remove(dbFilename.c_str());

    cout << "  - Creando Storage Manager (Archivo: " << dbFilename << ")..." << endl;
    StorageManager storageManager(dbFilename);

    // Buffer Pool con capacidad de 20 páginas.
    // Nota: con maxKeys=3 el árbol puede alcanzar ~4 niveles con 100 registros,
    // por lo que necesitamos capacidad suficiente para mantener el camino raíz-hoja
    // más las páginas de splits sin evictar nodos en medio de una inserción.
    // La demo de LRU (sección E) usará una función auxiliar con un pool pequeño.
    int bufferCapacity = 20;
    cout << "  - Creando Buffer Manager (Capacidad: " << bufferCapacity << " paginas)..." << endl;
    BufferManager bufferManager(bufferCapacity, &storageManager);

    // B+ Tree con maxKeys = 3 (tanto para hojas como para internos)
    // Esto asegura múltiples niveles y divisiones (splits) con 100 registros.
    int maxKeysLeaf = 3;
    int maxKeysInternal = 3;
    cout << "  - Creando B+ Tree (Orden: max_keys_leaf=" << maxKeysLeaf
         << ", max_keys_internal=" << maxKeysInternal << ")..." << endl;
    BPlusTree bplusTree(&bufferManager, &storageManager, maxKeysLeaf, maxKeysInternal);

    // =====================================================================
    // B. Inserción de datos de prueba
    // =====================================================================
    cout << "\n[B] GENERANDO E INSERTANDO REGISTROS DE PRUEBA..." << endl;
    cout << "  - Generando 100 registros de alumnos..." << endl;

    int currentDataPageId = 1; // Reservar página 0 como metadatos del índice B+
    vector<pair<int, index_m::RID>> indexedRecords;

    // Generamos claves del 1 al 100
    vector<int> keys;
    for (int i = 1; i <= 100; ++i) {
        keys.push_back(i);
    }

    // Barajamos las claves de forma determinista para la demo
    // Esto demostrará la flexibilidad de inserción aleatoria y balanceo del árbol
    for (size_t i = 0; i < keys.size(); ++i) {
        size_t j = (i * 31 + 7) % keys.size();
        swap(keys[i], keys[j]);
    }

    // Insertar registros en disco
    for (int key : keys) {
        string recordData = "AlumnoID: " + to_string(key) + ", Codigo: 2026-" + to_string(100000 + key) + ", Nota: " + to_string(10 + (key % 11));
        TableRID trid = InsertRecordToTable(bufferManager, recordData, currentDataPageId);

        index_m::RID irid;
        irid.pageId = trid.pageId;
        irid.slotId = trid.slotId;

        indexedRecords.push_back({key, irid});
    }

    cout << "  - Insercion finalizada. Registros almacenados en las paginas 1 a " << currentDataPageId << "." << endl;
    cout << "  - Se grabaron " << keys.size() << " registros con exito." << endl;

    // =====================================================================
    // C. Construcción del índice B+ Tree
    // =====================================================================
    // IMPORTANTE: Forzar que todas las páginas de datos estén en disco antes de
    // comenzar el árbol. Así GetNumPages() devuelve el conteo real de páginas
    // físicas y AllocatePage no colisiona con las páginas de datos (0 y 1).
    cout << "\n  - Sincronizando paginas de datos a disco antes de construir el indice..." << endl;
    bufferManager.Flush();
    cout << "  - Paginas de datos en disco: " << storageManager.GetNumPages() << endl;

    cout << "\n[C] CONSTRUYENDO INDICE B+ TREE..." << endl;
    cout << "  - Insertando llaves y RIDs en el arbol..." << endl;

    for (const auto& record : indexedRecords) {
        bplusTree.Insert(record.first, record.second);
    }

    cout << "  - Indice construido correctamente." << endl;
    cout << "  - Estructura basica del B+ Tree en persistencia:" << endl;
    bplusTree.PrintTree();

    cout << "\n[D] DEMOSTRACION DE ELIMINACION DEL B+ TREE..." << endl;
    cout << "  - Claves seleccionadas para eliminar: 1, 7, 15, 42, 88, 99" << endl;
    cout << "  - Estado del arbol antes de las eliminaciones:" << endl;
    bplusTree.PrintTree();

    vector<int> keysToDelete = {1, 7, 15, 42, 88, 99};
    for (int key : keysToDelete) {
        bool deleted = bplusTree.Delete(key);
        cout << "  - Eliminando clave [" << key << "]... " << (deleted ? "OK" : "FAIL") << endl;
    }

    cout << "  - Estado del arbol despues de las eliminaciones:" << endl;
    bplusTree.PrintTree();

    cout << "  - Verificando busquedas tras la eliminacion..." << endl;
    for (int key : keysToDelete) {
        index_m::RID foundRID = bplusTree.Search(key);
        cout << "    * Clave [" << key << "] => " << (foundRID.IsValid() ? "AUN EXISTE" : "ELIMINADA") << endl;
    }

    cout << "  - Persistiendo cambios del arbol y recargando desde disco..." << endl;
    bufferManager.Flush();
    {
        storage::StorageManager storageManagerReloaded(dbFilename);
        buffer::BufferManager bufferManagerReloaded(bufferCapacity, &storageManagerReloaded);
        BPlusTree loadedTree(&bufferManagerReloaded, &storageManagerReloaded, maxKeysLeaf, maxKeysInternal);

        cout << "  - Arbol recargado desde disco. Root page id = " << loadedTree.GetRootPageId() << endl;
        loadedTree.PrintTree();

        cout << "  - Validando persistencia tras eliminacion:" << endl;
        for (int key : {15, 42, 88}) {
            index_m::RID foundRID = loadedTree.Search(key);
            cout << "    * Clave [" << key << "] en arbol recargado => " << (foundRID.IsValid() ? "AUN EXISTE" : "ELIMINADA") << endl;
        }
        for (int key : {20, 50, 100}) {
            index_m::RID foundRID = loadedTree.Search(key);
            cout << "    * Clave [" << key << "] en arbol recargado => " << (foundRID.IsValid() ? "AUN EXISTE" : "ELIMINADA") << endl;
        }

        bufferManagerReloaded.Flush();
    }

    cout << "\n  - Reabriendo el manager de almacenamiento para verificar persistencia..." << endl;
    bufferManager.Flush();
    {
        storage::StorageManager storageManagerReloaded(dbFilename);
        buffer::BufferManager bufferManagerReloaded(bufferCapacity, &storageManagerReloaded);
        BPlusTree loadedTree(&bufferManagerReloaded, &storageManagerReloaded, maxKeysLeaf, maxKeysInternal);
        cout << "  - B+ Tree recargado desde disco. Root page id = " << loadedTree.GetRootPageId() << endl;
        loadedTree.PrintTree();

        cout << "\n[E] EJECUTANDO PRUEBAS DE BUSQUEDA EN EL ARBOL RECARGADO..." << endl;
        vector<int> searchKeys = {42, 15, 88, 999}; // 999 no existe
        for (int key : searchKeys) {
            cout << "  - Buscando clave [" << key << "]..." << endl;
            index_m::RID foundRID = loadedTree.Search(key);

            if (foundRID.IsValid()) {
                cout << "    * ENCONTRADA! RID = (Pagina: " << foundRID.pageId << ", Slot: " << foundRID.slotId << ")" << endl;
                Frame* dataFrame = bufferManagerReloaded.GetPage(foundRID.pageId);
                vector<uint8_t> recordBytes = dataFrame->page->ReadRecord(foundRID.slotId);
                bufferManagerReloaded.ReleasePage(foundRID.pageId, false);

                string recordText(recordBytes.begin(), recordBytes.end());
                cout << "    * Contenido del Registro: \"" << recordText << "\"" << endl;
            } else {
                cout << "    * NO ENCONTRADA! La clave [" << key << "] no existe en el indice." << endl;
            }
        }

        cout << "\n  - Flush del B+ Tree recargado..." << endl;
        bufferManagerReloaded.Flush();
    }
    // =====================================================================
    // D. Pruebas de búsqueda finales con el árbol activo
    // =====================================================================
    cout << "\n[F] EJECUTANDO PRUEBAS DE BUSQUEDA EN EL ARBOL..." << endl;

    vector<int> searchKeys = {42, 15, 88, 999}; // 999 no existe
    for (int key : searchKeys) {
        cout << "  - Buscando clave [" << key << "]..." << endl;
        index_m::RID foundRID = bplusTree.Search(key);

        if (foundRID.IsValid()) {
            cout << "    * ENCONTRADA! RID = (Pagina: " << foundRID.pageId << ", Slot: " << foundRID.slotId << ")" << endl;
            // Recuperar el registro real desde la página usando el Buffer Manager
            Frame* dataFrame = bufferManager.GetPage(foundRID.pageId);
            vector<uint8_t> recordBytes = dataFrame->page->ReadRecord(foundRID.slotId);
            bufferManager.ReleasePage(foundRID.pageId, false); // Liberar sin marcar sucia

            string recordText(recordBytes.begin(), recordBytes.end());
            cout << "    * Contenido del Registro: \"" << recordText << "\"" << endl;
        } else {
            cout << "    * NO ENCONTRADA! La clave [" << key << "] no existe en el indice." << endl;
        }
    }

    // =====================================================================
    // E. Pruebas del Buffer Manager y LRU
    // =====================================================================
    cout << "\n[G] PRUEBAS DE FUNCIONAMIENTO DEL BUFFER MANAGER Y REEMPLAZO LRU..." << endl;
    cout << "    (Se crea un Buffer Manager auxiliar con capacidad=5 para demostrar" << endl;
    cout << "     el reemplazo LRU sin interferir con la construccion del indice)" << endl;

    // Guardar a disco todo lo que quede pendiente en el pool principal
    bufferManager.Flush();

    // Pool auxiliar de 5 páginas para la demo de LRU
    BufferManager lruDemo(5, &storageManager);

    cout << "\n  Estado inicial del pool de demo (vacio):" << endl;
    lruDemo.PrintStatus();

    // Secuencia de accesos diseñada para forzar reemplazos LRU visibles
    cout << "  Secuencia de accesos a paginas (pool=5, se forzaran reemplazos):" << endl;
    vector<int> pagesToAccess = {0, 1, 2, 3, 4, 5, 6, 7, 8, 2, 0};
    int totalPages = storageManager.GetNumPages();

    for (int pid : pagesToAccess) {
        if (pid >= totalPages) continue;
        cout << "  -> Solicitando pagina " << pid << "..." << endl;
        Frame* f = lruDemo.GetPage(pid);
        
        if (pid == 2) {
            cout << "     [Pin Count: " << f->pinCount << " | DIRTY: marcando como modificada]" << endl;
            lruDemo.ReleasePage(pid, true);
        } else {
            cout << "     [Pin Count: " << f->pinCount << " | CLEAN]" << endl;
            lruDemo.ReleasePage(pid, false);
        }
    }

    cout << "\n  Estado del pool tras la secuencia de accesos:" << endl;
    lruDemo.PrintStatus();

    // Mostrar página pinned/unpinned manualmente
    cout << "  Fijando (pin) la pagina 0 sin liberarla..." << endl;
    Frame* pinnedFrame = lruDemo.GetPage(0);
    cout << "  -> Pagina 0 fijada. Pin count: " << pinnedFrame->pinCount << endl;
    cout << "  Solicitando paginas 10, 11, 12 con la pagina 0 pinned..." << endl;
    for (int pid : {10, 11, 12}) {
        if (pid >= totalPages) continue;
        Frame* tf = lruDemo.GetPage(pid);
        lruDemo.ReleasePage(pid, false);
    }
    cout << "  Estado (pagina 0 sigue en pool porque esta pinned):" << endl;
    lruDemo.PrintStatus();

    cout << "  Liberando (unpin) la pagina 0..." << endl;
    lruDemo.ReleasePage(0, false);
    cout << "  -> Pagina 0 liberada. Ahora es candidata a ser reemplazada." << endl;

    cout << "  Forzando reemplazo de pagina 0 solicitando una nueva pagina..." << endl;
    if (13 < totalPages) {
        Frame* tf = lruDemo.GetPage(13);
        lruDemo.ReleasePage(13, false);
    }
    cout << "  Estado final del pool de demo:" << endl;
    lruDemo.PrintStatus();

    cout << "  Ejecutando Flush del pool de demo..." << endl;
    lruDemo.Flush();
    cout << "  Flush completado. Paginas dirty escritas a disco." << endl;

    // =====================================================================
    // F. Estadísticas finales
    // =====================================================================
    cout << "\n[H] ESTADISTICAS FINALES DEL SISTEMA" << endl;
    cout << "---------------------------------------------------------------------" << endl;
    cout << "  - Cantidad total de paginas en disco:     " << storageManager.GetNumPages() << endl;
    cout << "  - Cantidad total de registros insertados: " << keys.size() << endl;
    cout << "\n  Estadisticas del Buffer Pool Principal (construccion del indice):" << endl;
    cout << "  - Accesos totales al Buffer Pool: " << bufferManager.GetAccessCount() << endl;
    cout << "  - Hits en memoria RAM:            " << bufferManager.GetHitCount() << endl;
    cout << "  - Misses (lecturas de disco):     " << bufferManager.GetMissCount() << endl;
    cout << "  - Hit Rate del Buffer Pool:       " << fixed << setprecision(2) << bufferManager.GetHitRate() * 100.0 << "%" << endl;
    cout << "\n  Estadisticas del Pool de Demo LRU (seccion E):" << endl;
    cout << "  - Accesos totales:  " << lruDemo.GetAccessCount() << endl;
    cout << "  - Hits:             " << lruDemo.GetHitCount() << endl;
    cout << "  - Misses:           " << lruDemo.GetMissCount() << endl;
    cout << "  - Hit Rate:         " << fixed << setprecision(2) << lruDemo.GetHitRate() * 100.0 << "%" << endl;

    auto end_time = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> elapsed = end_time - start_time;
    cout << "  - Tiempo total de ejecucion: " << elapsed.count() << " ms" << endl;
    cout << "---------------------------------------------------------------------" << endl;

    cout << "\nEjecucion finalizada correctamente. SGBD en estado consistente." << endl;
    return 0;
}
