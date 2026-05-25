#include <iostream>
#include <string>
#include <vector>

#include "storage/storage.h"
#include "buffer/buffer.h"

using namespace std;

int main() {

    // =========================================================
    // SEMANA 4
    // Insercion y recuperacion de registros por slots
    // =========================================================

    cout << "===== SEMANA 4 =====" << endl;
    cout << "Insercion y recuperacion de registros" << endl;

    storage::Page pagina;

    string alumno1 = "Alumno: Carlos";
    string alumno2 = "Alumno: Ana";
    string alumno3 = "Alumno: Diego";

    pagina.InsertRecord(vector<uint8_t>(alumno1.begin(), alumno1.end()));
    pagina.InsertRecord(vector<uint8_t>(alumno2.begin(), alumno2.end()));
    pagina.InsertRecord(vector<uint8_t>(alumno3.begin(), alumno3.end()));

    cout << "\nRegistros insertados en la pagina:" << endl;

    for (const auto& registro : pagina.ReadAllRecords()) {

        string texto(registro.begin(), registro.end());

        cout << "- " << texto << endl;
    }

    // =========================================================
    // SEMANA 5
    // Storage Manager - Persistencia en disco
    // =========================================================

    cout << "\n===== SEMANA 5 =====" << endl;
    cout << "Storage Manager - Escritura y lectura en disco" << endl;

    storage::StorageManager storageManager("../data/database.db");

    // Guardar pagina en disco
    if (!storageManager.WritePageData(0, pagina)) {

        cerr << "Error al guardar pagina en disco" << endl;
        return 1;
    }

    cout << "Pagina 0 guardada correctamente en disco" << endl;

    // Leer pagina desde disco
    auto paginaRecuperada = storageManager.ReadPageData(0);

    if (!paginaRecuperada) {

        cerr << "Error al recuperar pagina desde disco" << endl;
        return 1;
    }

    cout << "\nContenido recuperado desde disco:" << endl;

    for (const auto& registro : paginaRecuperada->ReadAllRecords()) {

        string texto(registro.begin(), registro.end());

        cout << "- " << texto << endl;
    }

    // =========================================================
    // SEMANA 6
    // Buffer Manager + Politica LRU
    // =========================================================

    cout << "\n===== SEMANA 6 =====" << endl;
    cout << "Buffer Manager con politica de reemplazo LRU" << endl;

    // Buffer con capacidad maxima de 2 paginas
    buffer::BufferManager bufferManager(2, &storageManager);

    // =========================================================
    // CARGA DE PAGINA 0
    // =========================================================

    cout << "\n[1] Solicitando pagina 0..." << endl;

    auto frame0 = bufferManager.GetPage(0);

    cout << "Pagina 0 cargada en memoria RAM" << endl;

    string curso1 = "Base de Datos";

    frame0->page->InsertRecord(
        vector<uint8_t>(curso1.begin(), curso1.end())
    );

    bufferManager.ReleasePage(0, true);

    cout << "Pagina 0 liberada y marcada como dirty" << endl;

    // =========================================================
    // CARGA DE PAGINA 1
    // =========================================================

    cout << "\n[2] Solicitando pagina 1..." << endl;

    auto frame1 = bufferManager.GetPage(1);

    cout << "Pagina 1 cargada en memoria RAM" << endl;

    string curso2 = "Sistemas Operativos";

    frame1->page->InsertRecord(
        vector<uint8_t>(curso2.begin(), curso2.end())
    );

    bufferManager.ReleasePage(1, true);

    cout << "Pagina 1 liberada y marcada como dirty" << endl;

    // =========================================================
    // CARGA DE PAGINA 2
    // AQUI SE EJECUTA LRU
    // =========================================================

    cout << "\n[3] Solicitando pagina 2..." << endl;

    cout << "El buffer esta lleno" << endl;
    cout << "Se ejecuta la politica LRU..." << endl;

    auto frame2 = bufferManager.GetPage(2);

    cout << "La pagina menos recientemente usada fue reemplazada"
         << endl;

    string curso3 = "Arquitectura de Computadoras";

    frame2->page->InsertRecord(
        vector<uint8_t>(curso3.begin(), curso3.end())
    );

    bufferManager.ReleasePage(2, true);

    cout << "Pagina 2 cargada correctamente" << endl;

    // =========================================================
    // FLUSH FINAL
    // =========================================================

    cout << "\nGuardando cambios pendientes en disco..." << endl;

    bufferManager.Flush();

    cout << "Flush ejecutado correctamente" << endl;

    // =========================================================
    // VERIFICACION FINAL
    // =========================================================

    cout << "\n===== VERIFICACION FINAL =====" << endl;

    for (int i = 0; i < 3; i++) {

        auto paginaFinal = storageManager.ReadPageData(i);

        cout << "\nPagina " << i << ":" << endl;

        if (paginaFinal) {

            auto registros = paginaFinal->ReadAllRecords();

            if (registros.empty()) {

                cout << "(Sin registros)" << endl;
            }

            for (const auto& registro : registros) {

                string texto(registro.begin(), registro.end());

                cout << "- " << texto << endl;
            }
        }
    }

    cout << "\nEjecucion finalizada correctamente." << endl;

    return 0;
}
