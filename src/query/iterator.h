#pragma once

#include "record.h"

namespace query {

    /**
     * Iterator — Interfaz base del Modelo Volcano (Iterator Model).
     *
     * Todo operador del procesador de consultas implementa esta interfaz.
     * El flujo de datos es pull-based: el operador superior llama a Next()
     * sobre el operador inferior hasta que éste devuelve false.
     *
     * Protocolo obligatorio:
     *   1. Open()  — Inicializa el estado interno; abre hijos si los hay.
     *   2. Next()  — Avanza al siguiente registro. Devuelve true si hay dato,
     *                false si la secuencia se agotó. El registro se escribe en 'record'.
     *   3. Close() — Libera recursos; cierra hijos si los hay.
     *
     * Regla: Open() debe llamarse antes de Next(); Close() debe llamarse
     * exactamente una vez cuando el operador ya no se necesite.
     */
    class Iterator {
    public:
        virtual void Open()              = 0;
        virtual bool Next(Record& record) = 0;
        virtual void Close()             = 0;
        virtual ~Iterator() {}
    };

} // namespace query
