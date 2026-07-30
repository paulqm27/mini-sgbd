
#include "query/record.h"
#include "query/scan_operator.h"
#include "query/select_operator.h"
#include "query/index_scan_operator.h"
#include "query/query_executor.h"
#include "storage/storage.h"
#include "buffer/buffer.h"
#include "index/bplus_tree.h"

#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <numeric>

using namespace std;
using namespace storage;
using namespace buffer;
using namespace index_m;
using namespace query;


struct Persona {
    int32_t id;
    int32_t edad;
    char    nombre[32];
};

static vector<uint8_t> SerializePersona(const Persona& p) {
    vector<uint8_t> buf(sizeof(Persona), 0);
    memcpy(buf.data(),     &p.id,    4);
    memcpy(buf.data() + 4, &p.edad,  4);
    memcpy(buf.data() + 8, p.nombre, 32);
    return buf;
}

static void InsertarPersonas(StorageManager& dataSM,
                             BufferManager&  dataBM,
                             BufferManager&  idxBM,
                             BPlusTree&      tree,
                             const vector<Persona>& personas)
{
    for (const auto& p : personas) {
        auto datos = SerializePersona(p);

        int numPages = dataSM.GetNumPages();
        int pageId   = -1;

        for (int i = 0; i < numPages; i++) {
            auto* frame = dataBM.GetPage(i);
            if (frame->page->InsertRecord(datos)) {
                int slotId = frame->page->GetNumSlots() - 1;
                dataBM.ReleasePage(i, true);
                pageId = i;
                tree.Insert(p.id, RID{ pageId, slotId });
                break;
            }
            dataBM.ReleasePage(i, false);
        }

        if (pageId == -1) {
            pageId = numPages;
            auto* frame = dataBM.GetPage(pageId);
            bool ok = frame->page->InsertRecord(datos);
            (void)ok;
            int slotId = frame->page->GetNumSlots() - 1;
            dataBM.ReleasePage(pageId, true);
            tree.Insert(p.id, RID{ pageId, slotId });
        }
    }
    dataBM.Flush();
    idxBM.Flush();
}


struct Metrics {
    long long  microseconds   = 0;
    int        bufferAccesses = 0;
    double     hitRate        = 0.0;
    bool       found          = false;
};

static Metrics MedirScanSelect(StorageManager& dataSM, BufferManager& dataBM,
                               int targetId)
{
    dataBM.ResetStats();

    auto t0 = chrono::high_resolution_clock::now();

    ScanOperator scan(&dataBM, &dataSM);
    SelectOperator select(&scan, [targetId](const Record& r) {
        if (r.data.size() < 4) return false;
        int32_t id = 0;
        memcpy(&id, r.data.data(), 4);
        return id == targetId;
    });

    select.Open();
    Record rec;
    bool found = select.Next(rec);
    select.Close();

    auto t1 = chrono::high_resolution_clock::now();
    long long us = chrono::duration_cast<chrono::microseconds>(t1 - t0).count();

    return { us, dataBM.GetAccessCount(), dataBM.GetHitRate(), found };
}

static Metrics MedirIndexScan(BufferManager& dataBM, BPlusTree& tree,
                              int targetId)
{
    dataBM.ResetStats();

    auto t0 = chrono::high_resolution_clock::now();
    IndexScanOperator idxScan(&tree, &dataBM, targetId);
    idxScan.Open();
    Record rec;
    bool found = idxScan.Next(rec);
    idxScan.Close();

    auto t1 = chrono::high_resolution_clock::now();
    long long us = chrono::duration_cast<chrono::microseconds>(t1 - t0).count();

    return { us, dataBM.GetAccessCount(), dataBM.GetHitRate(), found };
}

static void ImprimirTablaComparativa(
    const vector<int>& ids,
    const vector<Metrics>& scanMetrics,
    const vector<Metrics>& indexMetrics)
{
    cout << "\n";
    cout << "+--------+----------------------------------------+----------------------------------------+" << endl;
    cout << "|  ID    |       SCAN SECUENCIAL (O(N))           |     INDEX SCAN B+ Tree (O(log N))      |" << endl;
    cout << "+--------+----------+--------------+--------------+----------+--------------+--------------+" << endl;
    cout << "|        | Tiempo   | Accesos BM   | Hit Rate (%) | Tiempo   | Accesos BM   | Hit Rate (%) |" << endl;
    cout << "|        |   (us)   |              |              |   (us)   |              |              |" << endl;
    cout << "+--------+----------+--------------+--------------+----------+--------------+--------------+" << endl;

    for (size_t i = 0; i < ids.size(); i++) {
        const auto& s = scanMetrics[i];
        const auto& x = indexMetrics[i];
        cout << "| " << setw(6) << ids[i] << " |"
             << setw(9)  << s.microseconds  << " |"
             << setw(13) << s.bufferAccesses << " |"
             << setw(12) << fixed << setprecision(1) << (s.hitRate * 100.0) << " |"
             << setw(9)  << x.microseconds  << " |"
             << setw(13) << x.bufferAccesses << " |"
             << setw(12) << fixed << setprecision(1) << (x.hitRate * 100.0) << " |"
             << endl;
    }

    cout << "+--------+----------+--------------+--------------+----------+--------------+--------------+" << endl;

    long long avgScanUs = 0, avgIdxUs = 0;
    int       avgScanAcc = 0, avgIdxAcc = 0;
    double    avgScanHit = 0.0, avgIdxHit = 0.0;
    for (size_t i = 0; i < ids.size(); i++) {
        avgScanUs  += scanMetrics[i].microseconds;
        avgIdxUs   += indexMetrics[i].microseconds;
        avgScanAcc += scanMetrics[i].bufferAccesses;
        avgIdxAcc  += indexMetrics[i].bufferAccesses;
        avgScanHit += scanMetrics[i].hitRate;
        avgIdxHit  += indexMetrics[i].hitRate;
    }
    size_t n = ids.size();

    cout << "| PROM   |"
         << setw(9)  << avgScanUs  / (long long)n << " |"
         << setw(13) << avgScanAcc / (int)n << " |"
         << setw(12) << fixed << setprecision(1) << (avgScanHit / n * 100.0) << " |"
         << setw(9)  << avgIdxUs   / (long long)n << " |"
         << setw(13) << avgIdxAcc  / (int)n << " |"
         << setw(12) << fixed << setprecision(1) << (avgIdxHit  / n * 100.0) << " |"
         << endl;

    cout << "+--------+----------+--------------+--------------+----------+--------------+--------------+" << endl;
}


int main() {
    cout << "=========================================================" << endl;
    cout << "  TEST DE RENDIMIENTO: Scan Secuencial vs B+ Tree Index  " << endl;
    cout << "=========================================================" << endl;

    const string DATA_FILE = "test_perf_data.db";
    const string IDX_FILE  = "test_perf_idx.db";
    remove(DATA_FILE.c_str());
    remove(IDX_FILE.c_str());

    const int N = 200;
    vector<Persona> personas;
    personas.reserve(N);
    for (int i = 1; i <= N; i++) {
        Persona p{};
        p.id   = i;
        p.edad = 15 + (i % 50);
        snprintf(p.nombre, sizeof(p.nombre), "Persona_%03d", i);
        personas.push_back(p);
    }

    for (int i = 0; i < N; i++) {
        int j = (i * 31 + 7) % N;
        swap(personas[i], personas[j]);
    }

    cout << "\nInsertando " << N << " personas..." << endl;

    StorageManager dataSM(DATA_FILE);
    BufferManager  dataBM(16, &dataSM);

    StorageManager idxSM(IDX_FILE);
    BufferManager  idxBM(16, &idxSM);
    BPlusTree      tree(&idxBM, &idxSM, 7, 7);

    InsertarPersonas(dataSM, dataBM, idxBM, tree, personas);

    int numPaginasDatos  = dataSM.GetNumPages();
    int numPaginasIndice = idxSM.GetNumPages();

    cout << "  Paginas de datos  : " << numPaginasDatos  << endl;
    cout << "  Paginas de indice : " << numPaginasIndice << endl;
    cout << "  Registros         : " << N << endl;

    vector<int> idsABuscar = { 1, 50, 100, 150, 200 };

    cout << "\nEjecutando busquedas comparativas..." << endl;

    vector<Metrics> scanMetrics, indexMetrics;

    for (int id : idsABuscar) {
        MedirScanSelect(dataSM, dataBM, id);
        MedirIndexScan(dataBM, tree, id);

        // Medicion real
        Metrics sm = MedirScanSelect(dataSM, dataBM, id);
        Metrics im = MedirIndexScan(dataBM, tree, id);

        scanMetrics.push_back(sm);
        indexMetrics.push_back(im);
    }

    ImprimirTablaComparativa(idsABuscar, scanMetrics, indexMetrics);

    cout << "\n=========================================================" << endl;
    cout << "  TEST DE RENDIMIENTO COMPLETADO" << endl;
    cout << "=========================================================" << endl;

    return 0;
}
