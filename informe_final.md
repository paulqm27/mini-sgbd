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


Siguientes pasos recomendados:

- Añadir más benchmarks y gráficas (exportar CSV y generar PNG).
- Escribir una suite de pruebas unitarias más completa (usar GoogleTest o Catch2).
- Implementar la heurística `index-aware` en el planificador y medir su impacto.
- Redactar informe final `informe_final.pdf` con figuras y recomendaciones.
