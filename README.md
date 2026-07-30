# Mini Sistema Gestor de Base de Datos (Mini-SGBD)

Implementación de un **Mini Sistema Gestor de Base de Datos (Mini-SGBD)** desarrollado en **C++17**, cuyo objetivo es comprender el funcionamiento interno de un Sistema Gestor de Base de Datos mediante la implementación de sus principales componentes: almacenamiento, administración de memoria, indexación y procesamiento de consultas.

---

## Características

- Gestión de almacenamiento mediante archivos binarios.
- Organización de registros utilizando **Slotted Page**.
- Páginas de tamaño fijo de **4096 bytes**.
- **Buffer Manager** con política de reemplazo **LRU**.
- Manejo de **Dirty Pages** y **Pin Count**.
- Índice persistente basado en **B+ Tree**.
- Motor de consultas basado en el modelo **Volcano Iterator**.
- Operadores relacionales:
  - Scan
  - Select
  - Project
  - Nested Loop Join
  - Index Scan
- **Query Executor** con selección automática de estrategia.
- Persistencia de datos.
- Suite de pruebas para validar todos los módulos.

---

# Arquitectura

```text
                 Aplicación
                      │
                      ▼
               Query Engine
                      │
                      ▼
                 B+ Tree Index
                      │
                      ▼
               Buffer Manager
                      │
                      ▼
              Storage Manager
                      │
                      ▼
           Archivos (.db / .idx)
```

---

## Componentes

### Storage Manager

Responsable de la gestión del almacenamiento físico del sistema.

- Lectura de páginas
- Escritura de páginas
- Persistencia en disco
- Slotted Page

---

### Buffer Manager

Administra la memoria principal mediante un Buffer Pool.

- Buffer Pool
- Política LRU
- Dirty Pages
- Pin Count

---

### B+ Tree

Implementa un índice persistente para optimizar el acceso a los registros.

Operaciones soportadas:

- Search
- Insert
- Delete
- Split
- Merge
- Rebalance

---

### Query Engine

Implementa el procesamiento de consultas utilizando el modelo de iteradores Volcano.

Operadores disponibles:

- ScanOperator
- SelectOperator
- ProjectOperator
- NestedLoopJoinOperator
- IndexScanOperator

---

### Query Executor

Selecciona automáticamente la estrategia de ejecución de una consulta.

- Full Table Scan
- Index Scan

---

# Tecnologías

- C++17
- Standard Template Library (STL)
- CMake
- GCC / G++
- Visual Studio Code
- Git
- GitHub

---

# Estructura del Proyecto

```text
Mini-SGBD/
│
├── buffer/
├── index/
├── query/
├── storage/
├── tests/
│
├── CMakeLists.txt
└── README.md
```

---

# Compilación

```bash
mkdir build
cd build

cmake ..

cmake --build .
```

---

# Ejecución

Ejecutar las pruebas disponibles:

```bash
./test_storage
./test_buffer_manager
./test_bplus_insert
./test_bplus_search
./test_bplus_delete
./test_query_operators
./test_performance
./test_persistence
```

---

# Módulos Implementados

| Módulo | Estado |
|---------|:------:|
| Storage Manager | ✅ |
| Slotted Page | ✅ |
| Buffer Manager | ✅ |
| Política LRU | ✅ |
| B+ Tree | ✅ |
| Persistencia | ✅ |
| Scan Operator | ✅ |
| Select Operator | ✅ |
| Project Operator | ✅ |
| Nested Loop Join | ✅ |
| Index Scan | ✅ |
| Query Executor | ✅ |

---

# Objetivo

Desarrollar un Mini Sistema Gestor de Base de Datos que permita comprender los principios fundamentales del funcionamiento interno de un SGBD, implementando desde cero los mecanismos de almacenamiento, administración de memoria, indexación y procesamiento de consultas.

---
