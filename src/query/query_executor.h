#pragma once

#include "record.h"
#include "iterator.h"
#include "project_operator.h"
#include "buffer/buffer.h"
#include "storage/storage.h"
#include "index/bplus_tree.h"

#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace query {

    // -------------------------------------------------------------------------
    // Predicate — Describe la condición WHERE de una consulta.
    // -------------------------------------------------------------------------

    /**
     * Predicate — Condición de filtrado para QueryExecutor.
     *
     * Tipos soportados:
     *
     *   NONE     : Sin filtro. Equivale a SELECT * FROM tabla.
     *
     *   EQ_ID    : Igualdad sobre la clave primaria (id). Permite al QueryExecutor
     *              elegir IndexScanOperator automáticamente.
     *              Ejemplo: WHERE id = 100
     *
     *   GENERAL  : Predicado arbitrario expresado como función C++.
     *              El QueryExecutor usará ScanOperator + SelectOperator.
     *              Ejemplo: WHERE edad > 18
     */
    struct Predicate {
        enum class Type {
            NONE,       ///< Sin filtro
            EQ_ID,      ///< WHERE id = idValue  (clave primaria entera)
            GENERAL     ///< Predicado arbitrario via función
        };

        Type type = Type::NONE;
        int  idValue = 0;                                  ///< Usado cuando type == EQ_ID
        std::function<bool(const Record&)> generalFn;      ///< Usado cuando type == GENERAL

        // Fábricas estáticas para construir predicados de forma legible.
        static Predicate None()   { return Predicate{ Type::NONE, 0, nullptr }; }
        static Predicate EqId(int id) { return Predicate{ Type::EQ_ID, id, nullptr }; }
        static Predicate General(std::function<bool(const Record&)> fn) {
            Predicate p;
            p.type      = Type::GENERAL;
            p.generalFn = std::move(fn);
            return p;
        }
    };

    // -------------------------------------------------------------------------
    // QueryPlan — Descripción declarativa de una consulta.
    // -------------------------------------------------------------------------

    /**
     * QueryPlan — Especificación de una consulta sin necesidad de SQL.
     *
     * Campos:
     *   predicate  : Condición WHERE (por defecto: sin filtro).
     *   columns    : Columnas a proyectar. Si está vacío: SELECT * (sin proyección).
     *   startPage  : Primera página a escanear (útil para particionar tablas grandes).
     */
    struct QueryPlan {
        Predicate              predicate  = Predicate::None();
        std::vector<ColumnDef> columns;    ///< Columnas para proyección (vacío = todas)
        int                    startPage  = 0;
    };

    // -------------------------------------------------------------------------
    // QueryResult — Resultado de una consulta ejecutada.
    // -------------------------------------------------------------------------

    struct QueryResult {
        std::vector<Record> records;
        int                 totalScanned = 0;   ///< Registros examinados (antes de filtros)
        double              hitRate      = 0.0; ///< Hit Rate del buffer al finalizar
        int                 pageAccesses = 0;   ///< Accesos al buffer manager
    };

    // -------------------------------------------------------------------------
    // QueryExecutor — Planificador y ejecutor de consultas.
    // -------------------------------------------------------------------------

    /**
     * QueryExecutor — Selecciona y ejecuta el plan de consulta óptimo.
     *
     * Lógica de selección de plan (simplificada):
     *
     *   WHERE id = valor  AND  existe B+ Tree  →  IndexScanOperator
     *   cualquier otro caso                    →  ScanOperator [+ SelectOperator]
     *
     * Las consultas se construyen directamente como objetos C++ (QueryPlan).
     * No se implementa un parser SQL.
     *
     * Uso típico:
     *   QueryExecutor executor(&bm, &sm, &bPlusTree);
     *
     *   // Buscar persona con id=50 usando índice
     *   auto result = executor.Execute({ Predicate::EqId(50) });
     *
     *   // Buscar personas con edad > 18 usando scan
     *   auto result = executor.Execute({
     *       Predicate::General([](const Record& r) {
     *           int32_t edad; std::memcpy(&edad, r.data.data()+4, 4);
     *           return edad > 18;
     *       })
     *   });
     */
    class QueryExecutor {
    public:
        /**
         * @param bufferManager  Buffer Manager del sistema.
         * @param storageManager Storage Manager del sistema.
         * @param bPlusTree      Árbol B+ (opcional; puede ser nullptr si no hay índice).
         */
        QueryExecutor(buffer::BufferManager*   bufferManager,
                      storage::StorageManager* storageManager,
                      index_m::BPlusTree*      bPlusTree = nullptr);

        /**
         * Ejecuta el plan de consulta y devuelve todos los registros resultantes.
         *
         * @param plan  Descripción declarativa de la consulta.
         * @return      QueryResult con los registros y estadísticas de ejecución.
         */
        QueryResult Execute(const QueryPlan& plan);

        /**
         * Explica el plan seleccionado (para debug / educativo).
         * Imprime en stdout qué operadores se usarán.
         */
        void Explain(const QueryPlan& plan) const;

    private:
        buffer::BufferManager*   bufferManager_;
        storage::StorageManager* storageManager_;
        index_m::BPlusTree*      bPlusTree_;

        // Decide si puede usar el índice para el plan dado.
        bool CanUseIndex(const QueryPlan& plan) const;

        // Construye el árbol de operadores para el plan dado.
        // El iterador raíz se devuelve como unique_ptr.
        std::unique_ptr<Iterator> BuildPlan(const QueryPlan& plan) const;
    };

} // namespace query
