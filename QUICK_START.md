# MINI SGBD - QUICK START GUIDE

## 🚀 Compilación Rápida

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

## ✅ Ejecutar Tests (2 minutos)

```bash
# Tests unitarios (GoogleTest) - 6 tests
./build/gtest_iterators

# Output esperado:
# [ PASSED  ] 6 tests.
```

## 📊 Ejecutar Benchmarks

```bash
# Buffer Manager hit rate (< 1 segundo)
./build/buffer_benchmark > benchmark.csv
python3 scripts/plot_benchmark_svg.py benchmark.csv

# Impacto de Índice - Scan vs IndexScan (< 1 segundo)
./build/benchmark_index_impact > benchmark_index_impact.csv
```

## 🎯 Ejecución Integrada (Semana 14-16)

```bash
# Demo unificado que muestra todo
./build/full_flow_demo

# Output: 6 pasos ejecutados (Scan->Select->Project, 
# B+ Tree, IndexScan, Join, Benchmark)
```

## 📖 Ver Informe

```bash
# Opción 1: HTML (recomendado para PDF)
open informe_final.html

# Opción 2: Markdown
cat informe_final.md

# Opción 3: Convertir HTML a PDF
# En navegador: Ctrl+P → "Guardar como PDF"
```

## 📈 Ver Gráficas

```bash
# Ubicación
ls docs/figures/

# Archivos:
# - buffer_hitrate.svg (Hit rate vs buffer size)
# - buffer_time.svg (Tiempo vs buffer size)

# Abrir en navegador
open docs/figures/buffer_hitrate.svg
```

## 📋 Archivos Clave

| Archivo | Descripción |
|---------|------------|
| `informe_final.md` | Informe técnico completo |
| `informe_final.html` | Informe web (imprimible a PDF) |
| `RESUMEN_FINAL.md` | Resumen ejecutivo con resultados |
| `CHECKLIST.txt` | Checklist de completación |
| `etapa_14-15-16.md` | Plan original de trabajo |

## 🎁 Deliverables Finales

### Código Fuente (src/execution/)
- `iterator.h` - Interface base Volcano
- `scan.{h,cpp}` - Operador Scan
- `select.{h,cpp}` - Operador Select
- `project.{h,cpp}` - Operador Project
- `nested_loop_join.{h,cpp}` - NestedLoopJoin
- `index_scan.{h,cpp}` - IndexScan
- `planner.{h,cpp}` - Optimizador

### Tests (tests/)
- `gtest_iterators.cpp` - 6 tests unitarios ✅
- `buffer_benchmark.cpp` - Medición BufferManager ✅
- `benchmark_index_impact.cpp` - Comparación Scan/IndexScan ✅
- `demo_full_flow.cpp` - Demo integrado ✅

### Resultados
- `benchmark.csv` - Hit rate vs buffer size
- `benchmark_index_impact.csv` - Scan vs IndexScan
- `docs/figures/buffer_hitrate.svg` - Gráfica 1
- `docs/figures/buffer_time.svg` - Gráfica 2

## 📊 Resultados en Números

| Métrica | Resultado |
|---------|-----------|
| Tests | **6/6 ✅** |
| Buffer hit rate | **0% → 80%** (buffer >= 10) |
| IndexScan mejora | **-50% a -92%** vs Scan |
| Compilación | **Sin errores ✅** |
| Documentación | **Completa ✅** |

## 🔍 Verificación Rápida (< 5 minutos)

```bash
# 1. Compilar
cmake -S . -B build && cmake --build build -j$(nproc)

# 2. Tests
./build/gtest_iterators
# Debe mostrar: [ PASSED  ] 6 tests.

# 3. Demo integrado
./build/full_flow_demo
# Debe mostrar: 6 pasos completados

# 4. Ver informe
cat RESUMEN_FINAL.md
```

---

**Proyecto**: Mini SGBD Semana 14-15-16
**Status**: ✅ COMPLETADO
**Última actualización**: 15 de Julio de 2026
