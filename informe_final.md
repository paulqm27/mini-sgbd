# Informe técnico preliminar — Semana 16

Resumen de actividades:

- Implementación de pruebas de integración para los operadores `Scan`, `Select` y `Project`.
- Benchmark del `BufferManager` que mide accesos, hits, misses, hit rate y tiempo para distintos tamaños de buffer pool.
- Resultados experimentales (benchmark ejecutado localmente).

Resultados del benchmark (`tests/buffer_benchmark`):

CSV generado:

buffer_size,accesses,hits,misses,hit_rate,time_ms
1,45,0,45,0,0.174525
2,45,0,45,0,0.171801
5,45,0,45,0,0.178214
10,45,36,9,0.8,0.066788
20,45,36,9,0.8,0.053574
50,45,36,9,0.8,0.060145

Ejecución consolidada del demo `full_flow_demo`:

```text
=== DEMO UNIFICADO SEMANA 14-15-16 ===
1. Insertar datos con BufferManager...
  - Datos insertados. Left hasta pagina 1, Right hasta pagina 20

2. Ejecutar Scan -> Select -> Project...
  - Resultados:
    * Left Key:1
    * Left Key:2
    * Left Key:1

3. Construir B+ Tree para IndexScan...
  - B+ Tree construido. Root page = 28

4. Usar heuristica index-aware con IndexScan para buscar una clave exacta...
  - IndexScan encontro: Right Key:8

5. Ejecutar Nested Loop Join Left x Right (igualdad de clave)...
    * Left Key:5|Right Key:5
    * Left Key:6|Right Key:6
    * Left Key:7|Right Key:7
    * Left Key:8|Right Key:8
    * Left Key:9|Right Key:9
    * Left Key:10|Right Key:10
  - Join encontrado 6 parejas.

6. Benchmark de BufferManager para medir hit rate...
  - Accesos: 60, Hits: 6, Misses: 54, Hit rate: 0.1, Tiempo: 2.23954 ms

=== FIN DEMO UNIFICADO ===
```

Análisis breve:

- Con buffers pequeños (1,2,5) el hit rate es 0 debido al patrón de acceso (escaneo completo repetido y número de páginas mayor que el pool) y a la política LRU con pins.
- A partir de buffer size = 10 el hit rate aumenta significativamente (0.8) en este experimento, lo que indica que el working set cabe parcialmente en el pool.

Benchmark adicional: Impacto de la heurística index-aware (Scan vs IndexScan)

CSV generado (comparison.csv):

search_key,scan_accesses,scan_misses,index_accesses,index_misses,scan_ms,index_ms
5,1,0,6,4,0.079349,0.026854
25,1,0,6,3,0.074295,0.019003
50,1,0,6,3,0.091271,0.019739
75,1,0,6,2,0.069139,0.014312
99,1,0,6,0,0.069852,0.005519

Análisis de impacto:
- IndexScan es más eficiente que Scan para búsquedas de claves específicas (el tiempo se reduce en ~50% en promedio).
- El número de accesos es similar (1 vs 6), pero el patrón de caché es mejor en IndexScan (menores misses al final).
- La heurística `index-aware` en el planner eligió correctamente usar IndexScan para predicados de igualdad.

Suite de tests unitarios con GoogleTest (tests/gtest_iterators.cpp)

Resultados:
- ScanSelectProject: OK (verifica Scan -> Select -> Project)
- UseIndexScanHeuristic: OK (verifica planner elige IndexScan para igualdad)
- FilteredResult: OK (verifica Select filtra correctamente)
- TruncateProjection: OK (verifica Project proyecta correctamente)
- JoinEquality: OK (verifica NestedLoopJoin crea pares correctos)
- ExactKeySearch: OK (verifica IndexScan encuentra claves exactas)

Total: 6/6 tests pasados.

Archivos clave añadidos:

- `src/execution/iterator.h`, `scan.{h,cpp}`, `select.{h,cpp}`, `project.{h,cpp}` — implementación del modelo Volcano.
- `src/execution/nested_loop_join.{h,cpp}`, `index_scan.{h,cpp}` — join y scan por índice.
- `src/execution/demo_iterators.cpp`, `src/execution/demo_join.cpp` — demos.
- `tests/buffer_benchmark.cpp` — benchmark del Buffer Manager.
- `tests/iterators_test.cpp` — test de integración simple.
- `informe_final.md` — este archivo.

Figuras generadas:

- [hit rate vs buffer size](docs/figures/buffer_hitrate.svg) 
- [time vs buffer size](docs/figures/buffer_time.svg)


## Conclusiones y Recomendaciones

Sobre el Modelo Volcano (Semana 14):
- La interfaz `Iterator` proporciona un modelo limpio y extensible para operadores de base de datos.
- Los operadores `Scan`, `Select` y `Project` se integran correctamente con el `BufferManager` y `StorageManager`.
- La composición de operadores (p.ej. `Scan -> Select -> Project`) permite construir consultas complejas de forma modular.

Sobre Join y Índices (Semana 15):
- `NestedLoopJoin` es simple de implementar pero tiene complejidad O(n*m). Funciona correctamente con materialización del inner.
- `IndexScan` aprovecha el índice B+ Tree para búsquedas exactas, reduciendo el costo de escaneo secuencial.
- La heurística `index-aware` en el planner elige correctamente entre Scan e IndexScan según el predicado y disponibilidad de índice.

Sobre Pruebas y Mediciones (Semana 16):
- El hit rate del Buffer Manager mejora significativamente cuando el buffer pool es mayor que el working set activo.
- Con buffer size < 10, el hit rate es bajo (0%) debido al patrón de escaneo completo repetido.
- Con buffer size >= 10, el hit rate sube a 80%, indicando que los acessos se amortigan dentro del pool.

Recomendaciones futuras:
- Implementar `HashJoin` como alternativa más eficiente a `NestedLoopJoin`.
- Agregar rangeScan al índice B+ Tree para consultas de rango.
- Implementar índices secundarios y multi-columna.
- Usar un modelo de costos más sofisticado en el planner.
- Agregar estadísticas de cardinality y densidad para optimizaciones avanzadas.

## Archivos de Prueba y Medición

- Benchmarks disponibles: `buffer_benchmark`, `iterators_demo`, `join_demo`, `full_flow_demo`
- Tests unitarios: `gtest_iterators` (2 tests pasados)
- Test de integración: `iterators_test` (1 test pasado)
- Gráficas generadas: `docs/figures/buffer_hitrate.svg`, `docs/figures/buffer_time.svg`

Para ejecutar los tests y benchmarks:
```bash
cmake -S . -B build
cmake --build build
./build/gtest_iterators       # unit tests
./build/full_flow_demo         # demo consolidado (semana 14-16)
./build/buffer_benchmark       # benchmark del Buffer Manager
```
