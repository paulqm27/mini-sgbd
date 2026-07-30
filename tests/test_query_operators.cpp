
#include "query/record.h"
#include "query/iterator.h"
#include "query/scan_operator.h"
#include "query/select_operator.h"
#include "query/project_operator.h"
#include "query/nested_loop_join_operator.h"
#include "query/index_scan_operator.h"
#include "query/query_executor.h"
#include "storage/storage.h"
#include "buffer/buffer.h"
#include "index/bplus_tree.h"

#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>

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

static Persona DeserializePersona(const vector<uint8_t>& buf) {
    Persona p{};
    if (buf.size() >= sizeof(Persona)) {
        memcpy(&p.id,    buf.data(),     4);
        memcpy(&p.edad,  buf.data() + 4, 4);
        memcpy(p.nombre, buf.data() + 8, 32);
    }
    return p;
}

static void ImprimirPersona(const Persona& p) {
    cout << "  [id=" << p.id << ", edad=" << p.edad << ", nombre=" << p.nombre << "]" << endl;
}

struct Curso {
    int32_t idCurso;
    int32_t idPersona;
    char    nombreCurso[32];
};

static vector<uint8_t> SerializeCurso(const Curso& c) {
    vector<uint8_t> buf(sizeof(Curso), 0);
    memcpy(buf.data(),     &c.idCurso,   4);
    memcpy(buf.data() + 4, &c.idPersona, 4);
    memcpy(buf.data() + 8, c.nombreCurso, 32);
    return buf;
}

static Curso DeserializeCurso(const vector<uint8_t>& buf) {
    Curso c{};
    if (buf.size() >= sizeof(Curso)) {
        memcpy(&c.idCurso,   buf.data(),     4);
        memcpy(&c.idPersona, buf.data() + 4, 4);
        memcpy(c.nombreCurso, buf.data() + 8, 32);
    }
    return c;
}

static void ImprimirCurso(const Curso& c) {
    cout << "  [idCurso=" << c.idCurso << ", idPersona=" << c.idPersona << ", nombreCurso=" << c.nombreCurso << "]" << endl;
}

// Columnas estándar de Persona
static const ColumnDef COL_ID     = { "id",     0,  4  };
static const ColumnDef COL_EDAD   = { "edad",   4,  4  };
static const ColumnDef COL_NOMBRE = { "nombre", 8,  32 };


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
            assert(ok && "No cabe registro en página nueva");
            int slotId = frame->page->GetNumSlots() - 1;
            dataBM.ReleasePage(pageId, true);
            tree.Insert(p.id, RID{ pageId, slotId });
        }
    }
    dataBM.Flush();
    idxBM.Flush();
}

static void InsertarCursos(StorageManager& dataSM,
                           BufferManager&  dataBM,
                           const vector<Curso>& cursos)
{
    for (const auto& c : cursos) {
        auto datos = SerializeCurso(c);

        int numPages = dataSM.GetNumPages();
        int pageId   = -1;

        for (int i = 0; i < numPages; i++) {
            auto* frame = dataBM.GetPage(i);
            if (frame->page->InsertRecord(datos)) {
                dataBM.ReleasePage(i, true);
                pageId = i;
                break;
            }
            dataBM.ReleasePage(i, false);
        }

        if (pageId == -1) {
            pageId = numPages;
            auto* frame = dataBM.GetPage(pageId);
            bool ok = frame->page->InsertRecord(datos);
            assert(ok && "No cabe registro en página nueva");
            dataBM.ReleasePage(pageId, true);
        }
    }
    dataBM.Flush();
}

void TestScan(StorageManager& dataSM, BufferManager& dataBM, int numEsperados) {
    cout << "\n--- Test 1: ScanOperator ---" << endl;

    ScanOperator scan(&dataBM, &dataSM);
    scan.Open();

    int count = 0;
    Record rec;
    while (scan.Next(rec)) {
        Persona p = DeserializePersona(rec.data);
        ImprimirPersona(p);
        count++;
    }
    scan.Close();

    cout << "  Total registros escaneados: " << count << endl;
    assert(count == numEsperados && "El numero de registros escaneados no coincide");
    cout << "TestScan PASSED!" << endl;
}

void TestSelect(StorageManager& dataSM, BufferManager& dataBM) {
    cout << "\n--- Test 2: SelectOperator (edad > 18) ---" << endl;

    ScanOperator scan(&dataBM, &dataSM);
    SelectOperator select(&scan, [](const Record& r) {
        int32_t edad = 0;
        if (r.data.size() >= 8) {
            memcpy(&edad, r.data.data() + 4, 4);
        }
        return edad > 18;
    });

    select.Open();

    int count = 0;
    Record rec;
    while (select.Next(rec)) {
        Persona p = DeserializePersona(rec.data);
        assert(p.edad > 18 && "Registro no deberia pasar el filtro");
        ImprimirPersona(p);
        count++;
    }
    select.Close();

    cout << "  Registros con edad > 18: " << count << endl;
    assert(count > 0 && "Deberia haber al menos un resultado");
    cout << "TestSelect PASSED!" << endl;
}

void TestProject(StorageManager& dataSM, BufferManager& dataBM) {
    cout << "\n--- Test 3: ProjectOperator (id, nombre) ---" << endl;

    ScanOperator scan(&dataBM, &dataSM);
    ProjectOperator project(&scan, { COL_ID, COL_NOMBRE });

    project.Open();

    int count = 0;
    Record rec;
    while (project.Next(rec)) {
        assert(rec.data.size() == 36 && "Tamano proyectado incorrecto");

        int32_t id = 0;
        memcpy(&id, rec.data.data(), 4);

        char nombre[33] = {};
        memcpy(nombre, rec.data.data() + 4, 32);

        cout << "  [id=" << id << ", nombre=" << nombre << "]" << endl;
        count++;
    }
    project.Close();

    cout << "  Total registros proyectados: " << count << endl;
    assert(count > 0);
    cout << "TestProject PASSED!" << endl;
}
void TestNestedLoopJoin(StorageManager& dataSM1, BufferManager& dataBM1,
                        StorageManager& dataSM2, BufferManager& dataBM2)
{
    cout << "\n--- Test 4: NestedLoopJoinOperator ---" << endl;
    cout << "    (Tabla Personas x Tabla Cursos, join por id)" << endl;

    ScanOperator scanLeft (&dataBM1, &dataSM1);
    ScanOperator scanRight(&dataBM2, &dataSM2);

    NestedLoopJoinOperator join(
        &scanLeft,
        &scanRight,
        [](const Record& l, const Record& r) {
            if (l.data.size() < sizeof(Persona) || r.data.size() < sizeof(Curso)) return false;
            Persona p = DeserializePersona(l.data);
            Curso c = DeserializeCurso(r.data);
            return p.id == c.idPersona;
        }
    );

    join.Open();

    cout << "---------------------------------------------------------" << endl;
    cout << left << setw(16) << "Nombre" << setw(10) << "Edad" << "Curso" << endl;
    cout << "---------------------------------------------------------" << endl;

    int count = 0;
    Record rec;
    while (join.Next(rec)) {
        assert(rec.data.size() == sizeof(Persona) + sizeof(Curso));

        vector<uint8_t> leftData(rec.data.begin(), rec.data.begin() + sizeof(Persona));
        vector<uint8_t> rightData(rec.data.begin() + sizeof(Persona), rec.data.end());

        Persona p = DeserializePersona(leftData);
        Curso c = DeserializeCurso(rightData);

        cout << left << setw(16) << p.nombre << setw(10) << p.edad << c.nombreCurso << endl;
        assert(p.id == c.idPersona && "Join produjo un par incorrecto");
        count++;
    }
    join.Close();

    cout << "---------------------------------------------------------" << endl;
    cout << "\n  Total de registros unidos: " << count << endl;
    assert(count == 5 && "El join deberia producir exactamente 5 combinaciones");
    cout << "TestNestedLoopJoin PASSED!" << endl;
}

void TestIndexScan(BufferManager& dataBM, BPlusTree& tree, int searchId,
                   const vector<Persona>& personas)
{
    cout << "\n--- Test 5: IndexScanOperator (id = " << searchId << ") ---" << endl;

    IndexScanOperator idxScan(&tree, &dataBM, searchId);
    idxScan.Open();

    Record rec;
    bool found = idxScan.Next(rec);
    idxScan.Close();

    if (found) {
        Persona p = DeserializePersona(rec.data);
        cout << "  Registro encontrado via B+ Tree:" << endl;
        ImprimirPersona(p);
        assert(p.id == searchId && "El id encontrado no coincide con la busqueda");
    } else {
        cout << "  Clave " << searchId << " no encontrada en el indice." << endl;
    }
    bool existeEnTabla = false;
    for (const auto& p : personas) {
        if (p.id == searchId) { existeEnTabla = true; break; }
    }
    assert(found == existeEnTabla && "El indice no coincide con los datos reales");

    cout << "TestIndexScan PASSED!" << endl;
}

int main() {
    cout << "=========================================================" << endl;
    cout << "  PRUEBAS DEL PROCESADOR DE CONSULTAS (Semanas 14 y 15)  " << endl;
    cout << "=========================================================" << endl;

    const string DATA_FILE  = "test_qops_personas.db";
    const string IDX_FILE   = "test_qops_personas.idx";
    const string DATA_FILE2 = "test_qops_cursos.db";

    remove(DATA_FILE.c_str());
    remove(IDX_FILE.c_str());
    remove(DATA_FILE2.c_str());

    vector<Persona> personas = {
        { 1,  15, "Ana"      },
        { 2,  22, "Bruno"    },
        { 3,   8, "Carlos"   },
        { 4,  30, "Diana"    },
        { 5,  17, "Emilio"   },
        { 6,  25, "Fernanda" },
        { 7,  10, "Gustavo"  },
        { 8,  35, "Helena"   },
    };

    vector<Curso> cursos = {
        { 101, 1, "Natacion" },
        { 102, 2, "Futbol"   },
        { 103, 2, "Ingles"   },
        { 104, 4, "Voley"    },
        { 105, 8, "Karate"   }
    };

    StorageManager dataSM(DATA_FILE);
    BufferManager  dataBM(10, &dataSM);

    StorageManager idxSM(IDX_FILE);
    BufferManager  idxBM(10, &idxSM);
    BPlusTree      tree(&idxBM, &idxSM, 5, 5);

    cout << "\nInsertando personas en tabla 1 + construyendo indice..." << endl;
    InsertarPersonas(dataSM, dataBM, idxBM, tree, personas);
    cout << "  Paginas de datos : " << dataSM.GetNumPages() << endl;
    cout << "  Paginas de indice: " << idxSM.GetNumPages() << endl;

    StorageManager dataSM2(DATA_FILE2);
    BufferManager  dataBM2(10, &dataSM2);
    cout << "Insertando cursos en tabla 2..." << endl;
    InsertarCursos(dataSM2, dataBM2, cursos);
    cout << "  Paginas de cursos: " << dataSM2.GetNumPages() << endl;

    TestScan(dataSM, dataBM, static_cast<int>(personas.size()));
    TestSelect(dataSM, dataBM);
    TestProject(dataSM, dataBM);
    TestNestedLoopJoin(dataSM, dataBM, dataSM2, dataBM2);
    TestIndexScan(dataBM, tree, 4, personas);
    TestIndexScan(dataBM, tree, 99, personas);

    cout << "\n=========================================================" << endl;
    cout << "  TODAS LAS PRUEBAS COMPLETADAS CON EXITO" << endl;
    cout << "=========================================================" << endl;

    return 0;
}
