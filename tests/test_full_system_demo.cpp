#include "storage/storage.h"
#include "storage/page.h"
#include "buffer/buffer.h"
#include "index/bplus_tree.h"
#include "query/record.h"
#include "query/iterator.h"
#include "query/scan_operator.h"
#include "query/select_operator.h"
#include "query/project_operator.h"
#include "query/nested_loop_join_operator.h"
#include "query/index_scan_operator.h"
#include "query/query_executor.h"


#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <algorithm>
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
static_assert(sizeof(Persona) == 40, "Persona debe ocupar exactamente 40 bytes");

struct Curso {
    int32_t idCurso;
    int32_t idPersona;
    char    nombreCurso[32];
};
static_assert(sizeof(Curso) == 40, "Curso debe ocupar exactamente 40 bytes");

static vector<uint8_t> SerializePersona(const Persona& p) {
    vector<uint8_t> buf(sizeof(Persona), 0);
    memcpy(buf.data(),     &p.id,    4);
    memcpy(buf.data() + 4, &p.edad,  4);
    memcpy(buf.data() + 8, p.nombre, 32);
    return buf;
}

static Persona DeserializePersona(const vector<uint8_t>& buf) {
    Persona p{};
    if (buf.size() >= sizeof(Persona)) {
        memcpy(&p.id,    buf.data(),     4);
        memcpy(&p.edad,  buf.data() + 4, 4);
        memcpy(p.nombre, buf.data() + 8, 32);
    }
    return p;
}

static vector<uint8_t> SerializeCurso(const Curso& c) {
    vector<uint8_t> buf(sizeof(Curso), 0);
    memcpy(buf.data(),     &c.idCurso,    4);
    memcpy(buf.data() + 4, &c.idPersona,  4);
    memcpy(buf.data() + 8, c.nombreCurso, 32);
    return buf;
}

static Curso DeserializeCurso(const vector<uint8_t>& buf) {
    Curso c{};
    if (buf.size() >= sizeof(Curso)) {
        memcpy(&c.idCurso,    buf.data(),     4);
        memcpy(&c.idPersona,  buf.data() + 4, 4);
        memcpy(c.nombreCurso, buf.data() + 8, 32);
    }
    return c;
}

static const ColumnDef COL_ID     = { "id",     0,  4  };
static const ColumnDef COL_EDAD   = { "edad",   4,  4  };
static const ColumnDef COL_NOMBRE = { "nombre", 8,  32 };


static constexpr int LINE_WIDTH = 65;


static void PrintSeparator(char c = '=') {
    cout << string(LINE_WIDTH, c) << "\n";
}


static void PrintSection(int num, const string& title) {
    cout << "\n";
    PrintSeparator();
    string label = "  [" + to_string(num) + "] " + title + "  ";
    int padding  = (LINE_WIDTH - static_cast<int>(label.size())) / 2;
    if (padding < 0) padding = 0;
    cout << string(padding, ' ') << label << "\n";
    PrintSeparator();
}


static void PrintStep(const string& msg) {
    cout << "  >> " << msg << "\n";
}

static void PrintResult(const string& label, bool ok) {
    cout << "  " << (ok ? "[OK]   " : "[FAIL] ") << label << "\n";
}


static bool InsertarPersona(StorageManager& dataSM,
                             BufferManager&  dataBM,
                             BPlusTree&      tree,
                             const Persona&  p)
{
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
            return true;
        }
        dataBM.ReleasePage(i, false);
    }

    pageId = numPages;
    auto* frame = dataBM.GetPage(pageId);
    bool ok = frame->page->InsertRecord(datos);
    if (!ok) {
        dataBM.ReleasePage(pageId, false);
        return false;
    }
    int slotId = frame->page->GetNumSlots() - 1;
    dataBM.ReleasePage(pageId, true);
    tree.Insert(p.id, RID{ pageId, slotId });
    return true;
}


static bool InsertarCurso(StorageManager& dataSM,
                           BufferManager&  dataBM,
                           const Curso&    c)
{
    auto datos = SerializeCurso(c);

    int numPages = dataSM.GetNumPages();

    for (int i = 0; i < numPages; i++) {
        auto* frame = dataBM.GetPage(i);
        if (frame->page->InsertRecord(datos)) {
            dataBM.ReleasePage(i, true);
            return true;
        }
        dataBM.ReleasePage(i, false);
    }

    int pageId = numPages;
    auto* frame = dataBM.GetPage(pageId);
    bool ok = frame->page->InsertRecord(datos);
    if (!ok) {
        dataBM.ReleasePage(pageId, false);
        return false;
    }
    dataBM.ReleasePage(pageId, true);
    return true;
}

struct BenchResult {
    double  ms      = 0.0;
    int     count   = 0;
};

static BenchResult MedirScanCompleto(StorageManager& dataSM,
                                     BufferManager&  dataBM)
{
    auto t0 = chrono::high_resolution_clock::now();

    ScanOperator scan(&dataBM, &dataSM);
    scan.Open();
    Record rec;
    int cnt = 0;
    while (scan.Next(rec)) cnt++;
    scan.Close();

    auto t1 = chrono::high_resolution_clock::now();
    double ms = chrono::duration<double, milli>(t1 - t0).count();
    return { ms, cnt };
}

static BenchResult MedirIndexSearch(BPlusTree& tree, int key,
                                     BufferManager& dataBM)
{
    auto t0 = chrono::high_resolution_clock::now();

    IndexScanOperator idxScan(&tree, &dataBM, key);
    idxScan.Open();
    Record rec;
    int cnt = 0;
    while (idxScan.Next(rec)) cnt++;
    idxScan.Close();

    auto t1 = chrono::high_resolution_clock::now();
    double ms = chrono::duration<double, milli>(t1 - t0).count();
    return { ms, cnt };
}

static bool TestPersistencia() {
    PrintSection(2, "PERSISTENCIA - Escritura y relectura desde disco");

    const string DB  = "demo_persist.db";
    const string IDX = "demo_persist.idx";
    remove(DB.c_str());
    remove(IDX.c_str());

    /* Datos de prueba */
    vector<pair<int, string>> studentRecords = {
        {10, "Alumno 10: Paul,  Nota: 18"},
        {20, "Alumno 20: Maria, Nota: 15"},
        {30, "Alumno 30: Juan,  Nota: 16"},
        {40, "Alumno 40: Lucia, Nota: 20"}
    };

    PrintStep("FASE 1 - Creando base de datos y escribiendo registros...");
    {
        StorageManager dataSM(DB);
        BufferManager  dataBM(10, &dataSM);

        StorageManager idxSM(IDX);
        BufferManager  idxBM(10, &idxSM);
        BPlusTree      tree(&idxBM, &idxSM, 3, 3);

        int dataPageId = 1;

        vector<pair<int, RID>> indexed;
        for (const auto& [key, text] : studentRecords) {
            vector<uint8_t> rec(text.begin(), text.end());
            auto* frame = dataBM.GetPage(dataPageId);
            if (!frame->page->InsertRecord(rec)) {
                dataBM.ReleasePage(dataPageId, false);
                dataPageId++;
                frame = dataBM.GetPage(dataPageId);
                frame->page->InsertRecord(rec);
            }
            int slotId = frame->page->GetNumSlots() - 1;
            dataBM.ReleasePage(dataPageId, true);
            indexed.push_back({key, RID{dataPageId, slotId}});
        }
        dataBM.Flush();

        for (const auto& [key, rid] : indexed) {
            tree.Insert(key, rid);
        }
        idxBM.Flush();

        PrintStep("Registros insertados: " + to_string(studentRecords.size()) +
                  "  |  Paginas de datos: " + to_string(dataSM.GetNumPages()) +
                  "  |  Paginas de indice: " + to_string(idxSM.GetNumPages()));
    }

    PrintStep("FASE 2 - Reabriendo desde disco y validando contenido...");
    bool allOk = true;
    {
        StorageManager dataSM(DB);
        BufferManager  dataBM(10, &dataSM);

        StorageManager idxSM(IDX);
        BufferManager  idxBM(10, &idxSM);
        BPlusTree      tree(&idxBM, &idxSM, 3, 3);

        for (const auto& [key, expectedText] : studentRecords) {
            RID rid = tree.Search(key);
            if (!rid.IsValid()) {
                cout << "  [FAIL] Clave " << key << " no encontrada en indice\n";
                allOk = false;
                continue;
            }
            auto* frame = dataBM.GetPage(rid.pageId);
            auto bytes  = frame->page->ReadRecord(rid.slotId);
            dataBM.ReleasePage(rid.pageId, false);

            string actualText(bytes.begin(), bytes.end());
            bool match = (actualText == expectedText);
            if (!match) allOk = false;

            cout << "  Clave [" << setw(2) << key << "] -> \""
                 << actualText << "\" "
                 << (match ? "[OK]" : "[FAIL]") << "\n";
        }
    }

    PrintResult("Persistencia: datos coinciden tras reabrir el archivo", allOk);
    return allOk;
}

static bool TestBufferManager() {
    PrintSection(3, "BUFFER MANAGER - Politica LRU (Least Recently Used)");

    const string DB = "demo_buffer.db";
    remove(DB.c_str());

    {
        StorageManager sm(DB);
        vector<uint8_t> empty(PAGE_SIZE, 0);
        for (int i = 0; i < 10; i++) sm.WritePage(i, empty);
    }

    StorageManager sm(DB);
    BufferManager bm(3, &sm);

    PrintStep("Accediendo a paginas 0, 1, 2 (llenando el pool)...");
    bm.GetPage(0); bm.ReleasePage(0, false);
    bm.GetPage(1); bm.ReleasePage(1, false);
    bm.GetPage(2); bm.ReleasePage(2, false);

    PrintStep("Accediendo a paginas 3, 4, 5 (forzando reemplazos LRU)...");
    bm.GetPage(3); bm.ReleasePage(3, false);
    bm.GetPage(4); bm.ReleasePage(4, false);
    bm.GetPage(5); bm.ReleasePage(5, false);

    PrintStep("Reutilizando paginas 3, 4, 5 (deben ser HIT)...");
    bm.ResetStats();
    bm.GetPage(3); bm.ReleasePage(3, false);
    bm.GetPage(4); bm.ReleasePage(4, false);
    bm.GetPage(5); bm.ReleasePage(5, false);
    int hits3   = bm.GetHitCount();
    int misses3 = bm.GetMissCount();

    PrintStep("Accediendo a pagina 0 (evicted, debe ser MISS)...");
    bm.ResetStats();
    bm.GetPage(0); bm.ReleasePage(0, false);
    int missEvicted = bm.GetMissCount();

    bm.ResetStats();
    for (int i = 1; i <= 5; i++) { bm.GetPage(i % 3 + 3); bm.ReleasePage(i % 3 + 3, false); }
    for (int i = 1; i <= 5; i++) { bm.GetPage(i % 3 + 3); bm.ReleasePage(i % 3 + 3, false); }

    cout << "\n";
    PrintSeparator('-');
    cout << "  Buffer Manager - Estadisticas:\n";
    PrintSeparator('-');
    cout << "  Accesos totales    : " << bm.GetAccessCount() << "\n";
    cout << "  Hits               : " << bm.GetHitCount()    << "\n";
    cout << "  Misses             : " << bm.GetMissCount()   << "\n";
    cout << "  Hit Rate           : " << fixed << setprecision(1)
         << (bm.GetHitRate() * 100.0) << " %\n";
    PrintSeparator('-');

    bool lruOk  = (hits3 == 3 && misses3 == 0);
    bool missOk = (missEvicted == 1);

    PrintResult("Hits en paginas recientes             (esperado 3/3)", lruOk);
    PrintResult("Miss en pagina evicted                (esperado 1/1)", missOk);
    return lruOk && missOk;
}

static bool TestBPlusInsercion(StorageManager& idxSM, BufferManager& idxBM,
                                BPlusTree& tree, int N = 1000)
{
    PrintSection(4, "B+ TREE - Insercion de " + to_string(N) + " claves");

    vector<int> keys;
    keys.reserve(N);
    for (int i = 1; i <= N; i++) keys.push_back(i);
    for (int i = 0; i < N; i++) {
        int j = (i * 31 + 7) % N;
        swap(keys[i], keys[j]);
    }

    PrintStep("Insertando " + to_string(N) + " claves...");

    const int STEP = N / 10;
    for (int idx = 0; idx < N; idx++) {
        int k = keys[idx];
        tree.Insert(k, RID{ 1, k });
        if ((idx + 1) % STEP == 0) {
            int pct = (idx + 1) * 100 / N;
            cout << "    [" << setw(3) << pct << "%] "
                 << (idx + 1) << " / " << N << " claves insertadas\n";
        }
    }
    idxBM.Flush();

    PrintStep("Verificando muestra de 20 claves...");
    bool allFound = true;
    vector<int> sample = {1, 50, 100, 200, 300, 400, 500, 600, 700, 800,
                          900, 950, 999, 1000, 10, 25, 75, 150, 250, 750};
    for (int k : sample) {
        if (k > N) continue;
        RID r = tree.Search(k);
        if (!r.IsValid()) {
            cout << "  [FAIL] Clave " << k << " no encontrada!\n";
            allFound = false;
        }
    }

    PrintResult("Insercion masiva B+ Tree (" + to_string(N) + " claves)", allFound);
    return allFound;
}

static bool TestBPlusBusqueda(BPlusTree& tree, int N) {
    PrintSection(5, "B+ TREE - Busqueda de claves existentes e inexistentes");

    vector<int> existentes = { 1, 100, 250, 500, 750, 999, N };
    bool allFound = true;

    cout << "  Claves existentes:\n";
    for (int k : existentes) {
        if (k > N) { k = N; }
        RID r = tree.Search(k);
        bool ok = r.IsValid();
        if (!ok) allFound = false;
        cout << "    Clave [" << setw(5) << k << "] -> "
             << (ok ? "ENCONTRADA  RID(" + to_string(r.pageId)
                      + ", " + to_string(r.slotId) + ")"
                    : "NO ENCONTRADA  [ERROR]") << "\n";
    }

    vector<int> inexistentes = { 0, N + 1, N + 100, 999999, -1 };
    bool noneFound = true;

    cout << "\n  Claves inexistentes (deben retornar NOT FOUND):\n";
    for (int k : inexistentes) {
        RID r = tree.Search(k);
        bool ok = !r.IsValid();
        if (!ok) noneFound = false;
        cout << "    Clave [" << setw(6) << k << "] -> "
             << (ok ? "NOT FOUND   [OK]" : "ENCONTRADA  [ERROR]") << "\n";
    }

    PrintResult("Busqueda claves existentes   (todas encontradas)", allFound);
    PrintResult("Busqueda claves inexistentes (ninguna encontrada)", noneFound);
    return allFound && noneFound;
}

static bool TestBPlusEliminacion(BPlusTree& tree, int N) {
    PrintSection(6, "B+ TREE - Eliminacion de claves");

    vector<int> toDelete = { 1, 50, 100, 500, 750, N };
    bool allDeleted = true;

    cout << "  Eliminando claves: ";
    for (int k : toDelete) cout << k << " ";
    cout << "\n";

    for (int k : toDelete) {
        bool ok = tree.Delete(k);
        if (!ok) { cout << "  [WARN] Clave " << k << " no pudo eliminarse\n"; }
    }

    cout << "\n  Verificando eliminacion:\n";
    for (int k : toDelete) {
        RID r   = tree.Search(k);
        bool ok = !r.IsValid();
        if (!ok) allDeleted = false;
        cout << "    Clave [" << setw(5) << k << "] -> "
             << (ok ? "NOT FOUND   [OK]" : "AUN EXISTE  [ERROR]") << "\n";
    }

    vector<int> surviving = { 10, 200, 300, 600, 900 };
    bool survivorsOk = true;
    cout << "\n  Claves restantes (no eliminadas, deben existir):\n";
    for (int k : surviving) {
        if (k > N) continue;
        RID r = tree.Search(k);
        bool ok = r.IsValid();
        if (!ok) survivorsOk = false;
        cout << "    Clave [" << setw(5) << k << "] -> "
             << (ok ? "EXISTE      [OK]" : "NO EXISTE   [ERROR]") << "\n";
    }

    PrintResult("Claves eliminadas ya no existen", allDeleted);
    PrintResult("Claves no eliminadas siguen existiendo", survivorsOk);
    return allDeleted && survivorsOk;
}

static bool TestScan(StorageManager& dataSM, BufferManager& dataBM,
                     int numEsperados)
{
    PrintSection(7, "SCAN OPERATOR - Recorrido completo (Volcano model)");

    ScanOperator scan(&dataBM, &dataSM);
    scan.Open();

    int count = 0;
    Record rec;
    while (scan.Next(rec)) {
        count++;
    }
    scan.Close();

    bool ok = (count == numEsperados);
    cout << "  Registros esperados : " << numEsperados << "\n";
    cout << "  Registros obtenidos : " << count        << "\n";
    PrintResult("ScanOperator recorrio todos los registros", ok);
    return ok;
}
static bool TestSelect(StorageManager& dataSM, BufferManager& dataBM) {
    PrintSection(8, "SELECT OPERATOR - Filtro: edad > 18");

    ScanOperator scan(&dataBM, &dataSM);
    SelectOperator select(&scan, [](const Record& r) {
        if (r.data.size() < 8) return false;
        int32_t edad = 0;
        memcpy(&edad, r.data.data() + 4, 4);
        return edad > 18;
    });

    select.Open();
    int count = 0;
    bool allValid = true;
    Record rec;
    while (select.Next(rec)) {
        if (rec.data.size() >= sizeof(Persona)) {
            Persona p = DeserializePersona(rec.data);
            if (p.edad <= 18) allValid = false;
            cout << "    [id=" << setw(3) << p.id
                 << ", edad=" << setw(3) << p.edad
                 << ", nombre=" << p.nombre << "]\n";
        }
        count++;
    }
    select.Close();

    bool ok = (count > 0 && allValid);
    cout << "  Registros con edad > 18 : " << count << "\n";
    PrintResult("SelectOperator filtro correctamente", ok);
    return ok;
}

static bool TestProject(StorageManager& dataSM, BufferManager& dataBM) {
    PrintSection(9, "PROJECT OPERATOR - Proyeccion: id + nombre");

    ScanOperator scan(&dataBM, &dataSM);
    ProjectOperator project(&scan, { COL_ID, COL_NOMBRE });

    project.Open();
    int count = 0;
    bool sizesOk = true;
    Record rec;
    while (project.Next(rec)) {
        if (rec.data.size() != 36) { sizesOk = false; }
        if (rec.data.size() == 36) {
            int32_t id = 0;
            memcpy(&id, rec.data.data(), 4);
            char nombre[33] = {};
            memcpy(nombre, rec.data.data() + 4, 32);
            cout << "    [id=" << setw(3) << id << ", nombre=" << nombre << "]\n";
        }
        count++;
    }
    project.Close();

    bool ok = (count > 0 && sizesOk);
    cout << "  Registros proyectados   : " << count << "\n";
    PrintResult("ProjectOperator proyecto correctamente (36 bytes/registro)", ok);
    return ok;
}

static bool TestNestedLoopJoin(StorageManager& dataSM1, BufferManager& dataBM1,
                                StorageManager& dataSM2, BufferManager& dataBM2,
                                int expectedJoins)
{
    PrintSection(10, "NESTED LOOP JOIN - Personas x Cursos (por id)");

    ScanOperator scanLeft (&dataBM1, &dataSM1);
    ScanOperator scanRight(&dataBM2, &dataSM2);

    /* Condicion: Persona.id == Curso.idPersona */
    NestedLoopJoinOperator join(
        &scanLeft,
        &scanRight,
        [](const Record& l, const Record& r) {
            if (l.data.size() < sizeof(Persona) ||
                r.data.size() < sizeof(Curso)) return false;
            Persona p = DeserializePersona(l.data);
            Curso   c = DeserializeCurso  (r.data);
            return p.id == c.idPersona;
        }
    );

    join.Open();

    PrintSeparator('-');
    cout << "  " << left
         << setw(14) << "Nombre"
         << setw(8)  << "Edad"
         << setw(20) << "Curso" << "\n";
    PrintSeparator('-');

    int count  = 0;
    bool condOk = true;
    Record rec;
    while (join.Next(rec)) {
        if (rec.data.size() >= sizeof(Persona) + sizeof(Curso)) {
            vector<uint8_t> ld(rec.data.begin(),
                               rec.data.begin() + sizeof(Persona));
            vector<uint8_t> rd(rec.data.begin() + sizeof(Persona),
                               rec.data.end());
            Persona p = DeserializePersona(ld);
            Curso   c = DeserializeCurso  (rd);
            if (p.id != c.idPersona) condOk = false;
            cout << "  " << left
                 << setw(14) << p.nombre
                 << setw(8)  << p.edad
                 << setw(20) << c.nombreCurso << "\n";
        }
        count++;
    }
    join.Close();

    PrintSeparator('-');
    cout << "  Total de pares unidos   : " << count << "\n";
    cout << "  Pares esperados         : " << expectedJoins << "\n";

    bool ok = (count == expectedJoins && condOk);
    PrintResult("NestedLoopJoin produjo los pares correctos", ok);
    return ok;
}

static bool TestIndexScan(BPlusTree& tree, BufferManager& dataBM,
                           const vector<Persona>& personas)
{
    PrintSection(11, "INDEX SCAN OPERATOR - Busqueda via B+ Tree");

    bool allOk = true;

    struct SearchCase { int key; bool shouldExist; };
    vector<SearchCase> cases = {
        { personas[0].id, true  },
        { personas[2].id, true  },
        { personas[5].id, true  },
        { 9999,           false },
        { -1,             false }
    };

    for (const auto& sc : cases) {
        IndexScanOperator idxScan(&tree, &dataBM, sc.key);
        idxScan.Open();
        Record rec;
        bool found = idxScan.Next(rec);
        idxScan.Close();

        bool ok = (found == sc.shouldExist);
        if (!ok) allOk = false;

        cout << "    Clave [" << setw(5) << sc.key << "] -> ";
        if (found) {
            Persona p = DeserializePersona(rec.data);
            cout << "ENCONTRADA  [id=" << p.id << ", nombre=" << p.nombre << "]";
        } else {
            cout << "NOT FOUND";
        }
        cout << "  " << (ok ? "[OK]" : "[ERROR]") << "\n";
    }

    PrintResult("IndexScanOperator retorno resultados correctos", allOk);
    return allOk;
}

static bool TestQueryExecutor(StorageManager& dataSM, BufferManager& dataBM,
                               StorageManager& idxSM,  BufferManager& idxBM,
                               BPlusTree& tree,
                               const vector<Persona>& personas)
{
    PrintSection(12, "QUERY EXECUTOR - Seleccion automatica de estrategia");

    bool allOk = true;
    int targetId = personas[3].id;

    cout << "  Consulta A: EqId=" << targetId
         << "  (QueryExecutor con B+ Tree disponible)\n";
    {
        QueryExecutor qe(&dataBM, &dataSM, &tree);
        QueryPlan plan;
        plan.predicate = Predicate::EqId(targetId);

        cout << "  ";
        qe.Explain(plan);

        QueryResult res = qe.Execute(plan);
        bool found = !res.records.empty();
        if (found) {
            Persona p = DeserializePersona(res.records[0].data);
            cout << "    Resultado: [id=" << p.id
                 << ", edad=" << p.edad
                 << ", nombre=" << p.nombre << "]\n";
        }
        cout << "  Registros encontrados   : " << res.records.size() << "\n";
        cout << "  Total escaneados        : " << res.totalScanned   << "\n";
        cout << "  Hit Rate buffer         : " << fixed << setprecision(1)
             << (res.hitRate * 100.0) << " %\n";

        bool ok = (found && res.records[0].data.size() == sizeof(Persona));
        if (!ok) allOk = false;
        PrintResult("IndexScan estrategia elegida y resultado correcto", ok);
    }

    cout << "\n";

    cout << "  Consulta B: predicado GENERAL  (sin indice -> Scan secuencial)\n";
    {
        QueryExecutor qeNoIdx(&dataBM, &dataSM, nullptr);
        QueryPlan plan;
        plan.predicate = Predicate::General([&](const Record& r) {
            if (r.data.size() < 8) return false;
            int32_t edad = 0;
            memcpy(&edad, r.data.data() + 4, 4);
            return edad > 20;
        });

        cout << "  ";
        qeNoIdx.Explain(plan);

        QueryResult res = qeNoIdx.Execute(plan);
        cout << "  Registros con edad > 20 : " << res.records.size() << "\n";
        cout << "  Total escaneados        : " << res.totalScanned   << "\n";

        bool ok = !res.records.empty();
        if (!ok) allOk = false;
        PrintResult("Sequential Scan elegido y resultado correcto", ok);
    }

    return allOk;
}

static bool TestBenchmark() {
    PrintSection(13, "BENCHMARK FINAL - Scan Secuencial vs. B+ Tree Index");

    struct DatasetResult {
        int    n;
        double scanMs;
        double indexMs;
        double speedup;
    };

    vector<DatasetResult> results;
    vector<int> sizes = { 10000, 50000, 100000 };

    const int ORDER = 50;

    for (int N : sizes) {
        cout << "\n  Preparando dataset de " << N << " registros...\n";

        const string DB  = "demo_bench_data.db";
        const string IDX = "demo_bench_idx.db";
        remove(DB.c_str());
        remove(IDX.c_str());

        {
            StorageManager dataSM(DB);
            BufferManager  dataBM(64, &dataSM);

            StorageManager idxSM(IDX);
            BufferManager  idxBM(64, &idxSM);
            BPlusTree      tree(&idxBM, &idxSM, ORDER, ORDER);

            cout << "    Insertando... ";
            cout.flush();
            for (int i = 1; i <= N; i++) {
                Persona p{};
                p.id   = i;
                p.edad = 15 + (i % 50);
                snprintf(p.nombre, sizeof(p.nombre), "Persona_%06d", i);

                auto datos = SerializePersona(p);
                int numPag = dataSM.GetNumPages();
                int pageId = -1;

                for (int pg = 0; pg < numPag; pg++) {
                    auto* fr = dataBM.GetPage(pg);
                    if (fr->page->InsertRecord(datos)) {
                        int slotId = fr->page->GetNumSlots() - 1;
                        dataBM.ReleasePage(pg, true);
                        pageId = pg;
                        tree.Insert(i, RID{ pageId, slotId });
                        break;
                    }
                    dataBM.ReleasePage(pg, false);
                }
                if (pageId == -1) {
                    pageId = numPag;
                    auto* fr = dataBM.GetPage(pageId);
                    fr->page->InsertRecord(datos);
                    int slotId = fr->page->GetNumSlots() - 1;
                    dataBM.ReleasePage(pageId, true);
                    tree.Insert(i, RID{ pageId, slotId });
                }
            }
            dataBM.Flush();
            idxBM.Flush();
            cout << "OK  ("
                 << dataSM.GetNumPages() << " pags. datos, "
                 << idxSM.GetNumPages()  << " pags. indice)\n";
        }

        {
            StorageManager dataSM(DB);
            BufferManager  dataBM(64, &dataSM);

            StorageManager idxSM(IDX);
            BufferManager  idxBM(64, &idxSM);
            BPlusTree      tree(&idxBM, &idxSM, ORDER, ORDER);

            int searchKey = N;

            {
                ScanOperator sc(&dataBM, &dataSM); sc.Open();
                Record r; while (sc.Next(r)) {}; sc.Close();
            }
            {
                IndexScanOperator is(&tree, &dataBM, searchKey);
                is.Open(); Record r; is.Next(r); is.Close();
            }
            BenchResult scanR = MedirScanCompleto(dataSM, dataBM);

            BenchResult idxR  = MedirIndexSearch(tree, searchKey, dataBM);

            double speedup = (idxR.ms > 0.0)
                             ? (scanR.ms / idxR.ms)
                             : 0.0;

            cout << "    Scan : " << fixed << setprecision(3) << scanR.ms
                 << " ms  (" << scanR.count << " registros)\n";
            cout << "    Index: " << fixed << setprecision(3) << idxR.ms
                 << " ms  (" << idxR.count << " registros)\n";
            cout << "    Speedup: " << fixed << setprecision(1)
                 << speedup << "x\n";

            results.push_back({ N, scanR.ms, idxR.ms, speedup });
        }

        remove(DB.c_str());
        remove(IDX.c_str());
    }
    cout << "\n";
    PrintSeparator('=');
    cout << "  TABLA RESUMEN - Scan vs. Index\n";
    PrintSeparator('=');
    cout << "  " << left
         << setw(10) << "DATASET"
         << setw(14) << "SCAN (ms)"
         << setw(14) << "INDEX (ms)"
         << setw(12) << "SPEEDUP"
         << "\n";
    PrintSeparator('-');
    for (const auto& r : results) {
        cout << "  " << left
             << setw(10) << r.n
             << setw(14) << fixed << setprecision(3) << r.scanMs
             << setw(14) << fixed << setprecision(3) << r.indexMs
             << setw(12) << fixed << setprecision(1) << r.speedup << "x"
             << "\n";
    }
    PrintSeparator('=');

    bool ok = !results.empty();
    PrintResult("Benchmark completado exitosamente", ok);
    return ok;
}


int main() {
    PrintSeparator('*');
    cout << "\n";
    cout << "  +----------------------------------------------------------+\n";
    cout << "  |    MINI SGBD -- Sistema Gestor de Base de Datos (C++)    |\n";
    cout << "  |            Demostracion Final Integral                   |\n";
    cout << "  |                Sustentacion del Proyecto                 |\n";
    cout << "  +----------------------------------------------------------+\n";
    cout << "\n";
    PrintSeparator('*');

    const string DATA_FILE   = "demo_main_data.db";
    const string IDX_FILE    = "demo_main_idx.db";
    const string CURSOS_FILE = "demo_main_cursos.db";

    remove(DATA_FILE.c_str());
    remove(IDX_FILE.c_str());
    remove(CURSOS_FILE.c_str());

    PrintSection(1, "INICIALIZACION - Creacion de componentes base");

    vector<Persona> personas = {
        {  1, 15, "Ana"       },
        {  2, 22, "Bruno"     },
        {  3,  8, "Carlos"    },
        {  4, 30, "Diana"     },
        {  5, 17, "Emilio"    },
        {  6, 25, "Fernanda"  },
        {  7, 10, "Gustavo"   },
        {  8, 35, "Helena"    },
        {  9, 28, "Ivan"      },
        { 10, 19, "Julia"     },
    };

    vector<Curso> cursos = {
        { 101, 1, "Natacion" },
        { 102, 2, "Futbol"   },
        { 103, 2, "Ingles"   },
        { 104, 4, "Voley"    },
        { 105, 8, "Karate"   },
    };

    StorageManager dataSM(DATA_FILE);
    BufferManager  dataBM(20, &dataSM);

    StorageManager idxSM(IDX_FILE);
    BufferManager  idxBM(20, &idxSM);
    BPlusTree      tree(&idxBM, &idxSM, 5, 5);

    StorageManager cursosSM(CURSOS_FILE);
    BufferManager  cursosBM(10, &cursosSM);

    PrintStep("Insertando " + to_string(personas.size()) +
              " personas en la tabla y construyendo indice B+...");
    for (const auto& p : personas) {
        bool ok = InsertarPersona(dataSM, dataBM, tree, p);
        assert(ok && "Fallo insertar persona");
    }
    dataBM.Flush();
    idxBM.Flush();

    PrintStep("Insertando " + to_string(cursos.size()) + " cursos...");
    for (const auto& c : cursos) {
        bool ok = InsertarCurso(cursosSM, cursosBM, c);
        assert(ok && "Fallo insertar curso");
    }
    cursosBM.Flush();

    cout << "  Paginas de datos       : " << dataSM.GetNumPages()   << "\n";
    cout << "  Paginas de indice      : " << idxSM.GetNumPages()    << "\n";
    cout << "  Paginas de cursos      : " << cursosSM.GetNumPages() << "\n";
    PrintResult("Inicializacion completada", true);
    struct SectionResult { string name; bool ok; };
    vector<SectionResult> report;

    report.push_back({ "Persistencia", TestPersistencia() });

    report.push_back({ "Buffer Manager (LRU)", TestBufferManager() });

    {
        const string IDX2 = "demo_bplus_test.db";
        remove(IDX2.c_str());
        StorageManager idxSM2(IDX2);
        BufferManager  idxBM2(40, &idxSM2);
        BPlusTree      tree2(&idxBM2, &idxSM2, 7, 7);
        const int BPLUS_N = 1000;

        report.push_back({ "B+ Tree Insercion",
            TestBPlusInsercion(idxSM2, idxBM2, tree2, BPLUS_N) });

        report.push_back({ "B+ Tree Busqueda",
            TestBPlusBusqueda(tree2, BPLUS_N) });

        report.push_back({ "B+ Tree Eliminacion",
            TestBPlusEliminacion(tree2, BPLUS_N) });

        remove(IDX2.c_str());
    }

    report.push_back({ "Scan Operator",
        TestScan(dataSM, dataBM, static_cast<int>(personas.size())) });

    report.push_back({ "Select Operator", TestSelect(dataSM, dataBM) });

    report.push_back({ "Project Operator", TestProject(dataSM, dataBM) });

    report.push_back({ "Nested Loop Join",
        TestNestedLoopJoin(dataSM, dataBM, cursosSM, cursosBM,
                           static_cast<int>(cursos.size())) });

    report.push_back({ "Index Scan Operator",
        TestIndexScan(tree, dataBM, personas) });

    report.push_back({ "Query Executor",
        TestQueryExecutor(dataSM, dataBM, idxSM, idxBM, tree, personas) });

    report.push_back({ "Benchmark Final", TestBenchmark() });


    PrintSection(14, "RESUMEN FINAL - Estado de todos los componentes");

    bool globalOk = true;
    for (const auto& r : report) {
        globalOk = globalOk && r.ok;
        cout << "  " << (r.ok ? "[OK]   " : "[ERROR]")
             << "  " << left << setw(30) << r.name
             << (r.ok ? "OK" : "FAIL") << "\n";
    }

    cout << "\n";
    PrintSeparator('=');
    if (globalOk) {
        cout << "\n";
        cout << "  [OK] TODOS LOS COMPONENTES FUNCIONAN CORRECTAMENTE\n";
        cout << "       El Mini-SGBD esta listo para la sustentacion.\n";
        cout << "\n";
    } else {
        cout << "\n";
        cout << "  [ERROR] ALGUNOS COMPONENTES PRESENTARON FALLAS.\n";
        cout << "          Revise los errores reportados arriba.\n";
        cout << "\n";
    }
    PrintSeparator('=');
    cout << "\n";
    remove(DATA_FILE.c_str());
    remove(IDX_FILE.c_str());
    remove(CURSOS_FILE.c_str());

    return globalOk ? 0 : 1;
}
