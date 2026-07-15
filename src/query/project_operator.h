#pragma once

#include "iterator.h"
#include "record.h"
#include <vector>
#include <string>

namespace query {

    /**
     * ColumnDef — Descriptor de una columna dentro de un registro de longitud fija.
     *
     * Cada columna queda definida por:
     *   - name   : nombre descriptivo (para debug / impresión)
     *   - offset : posición en bytes desde el inicio del registro (record.data)
     *   - size   : tamaño en bytes de la columna
     *
     * Ejemplo para struct Persona { int32_t id; int32_t edad; char nombre[32]; }:
     *   ColumnDef{ "id",     0, 4  }
     *   ColumnDef{ "edad",   4, 4  }
     *   ColumnDef{ "nombre", 8, 32 }
     */
    struct ColumnDef {
        std::string name;
        int offset;
        int size;
    };

    /**
     * ProjectOperator — Proyección de columnas (π en álgebra relacional).
     *
     * Recibe un iterador hijo y una lista de columnas a proyectar.
     * En cada llamada a Next(), obtiene un registro del hijo y reescribe
     * record.data concatenando únicamente los bytes de las columnas seleccionadas.
     *
     * El resultado es un nuevo vector<uint8_t> con las columnas en el orden
     * en que fueron especificadas en 'columns'.
     *
     * Ejemplo de uso:
     *   // Proyectar solo id y nombre
     *   std::vector<ColumnDef> cols = {
     *       { "id",     0, 4  },
     *       { "nombre", 8, 32 }
     *   };
     *   auto project = std::make_unique<ProjectOperator>(scan.get(), cols);
     */
    class ProjectOperator : public Iterator {
    public:
        /**
         * @param child   Iterador que produce los registros completos.
         * @param columns Columnas a proyectar, en orden de aparición en la salida.
         */
        ProjectOperator(Iterator* child, std::vector<ColumnDef> columns);

        void Open()               override;
        bool Next(Record& record) override;
        void Close()              override;

        // Acceso a las definiciones de columnas (útil para impresión).
        const std::vector<ColumnDef>& GetColumns() const { return columns_; }

    private:
        Iterator*             child_;
        std::vector<ColumnDef> columns_;

        // Extrae los bytes de las columnas seleccionadas del registro fuente.
        std::vector<uint8_t> Project(const std::vector<uint8_t>& source) const;
    };

} // namespace query
