# Sustentación técnica del Mini SGBD

## 1. Resumen ejecutivo

Este proyecto implementa un **Mini Sistema Gestor de Base de Datos (Mini SGBD)** en C++17. El objetivo no es reemplazar a un motor comercial, sino demostrar de forma integrada los mecanismos fundamentales de un SGBD: persistencia en disco, gestión de páginas en memoria, indexación persistente y ejecución modular de consultas.

La solución está organizada en cuatro capas:

```text
Consulta / demo
        │
Operadores Volcano: Scan, Select, Project, IndexScan, NestedLoopJoin
        │
Índice B+ Tree ────────────────┐
        │                       │
        └────── Buffer Manager (Buffer Pool + LRU + pin/dirty)
                                │
                    Storage Manager / Page
                                │
                    archivos binarios de 4 KB
```

El recorrido principal es el siguiente: los registros se insertan en páginas de datos, el `BufferManager` conserva los cambios en memoria y los sincroniza a un archivo binario; el índice B+ almacena pares **clave → RID** (identificador físico de registro), y los operadores de ejecución usan el mismo buffer para leer los datos. Por ello, el índice y las consultas no dependen de una estructura completamente residente en RAM.

## 2. Objetivo y alcance

El sistema permite:

- persistir páginas y registros de longitud variable en disco;
- administrar un buffer pool de capacidad configurable;
- aplicar reemplazo LRU y controlar páginas fijadas (`pinned`) o modificadas (`dirty`);
- crear, buscar e insertar claves en un B+ Tree persistente, además de soportar eliminación con redistribución/fusión;
- ejecutar planes de consulta con el modelo Volcano: `open()`, `next()` y `close()`;
- aplicar selección, proyección, búsqueda por índice y `Nested Loop Join`.

El proyecto trabaja con una representación de registros como `std::vector<uint8_t>`. Los ejemplos serializan registros como texto para hacer visible la demostración; la infraestructura, sin embargo, opera sobre bytes y no depende del formato textual.

## 3. Cumplimiento de las consideraciones técnicas obligatorias

| Requisito obligatorio | Implementación y evidencia |
|---|---|
| Datos persistentes en archivos binarios | `StorageManager` abre el archivo con `std::ios::binary` y lee/escribe páginas completas mediante `ReadPage` y `WritePage` en `src/storage/storage.cpp`. |
| Página de tamaño fijo | `PAGE_SIZE = 4096` bytes en `src/storage/page.h`. Toda lectura/escritura de página usa ese tamaño. |
| Slot Directory | `Page` mantiene cabecera y entradas `Slot { offset, size }`; `InsertRecord`, `ReadRecord` y `ReadAllRecords` están en `src/storage/page.cpp`. |
| Buffer Pool configurable | `BufferManager(int capacity, StorageManager*)` recibe su capacidad al construirse (`src/buffer/buffer.h`). |
| Reemplazo LRU obligatorio | `lruList_` conserva el orden de uso. `UpdateLRU` mueve la página más recientemente usada al final y `ReplacePage` recorre desde el inicio para expulsar la menos reciente no fijada. |
| Estados `pinned` y `dirty` | Cada `Frame` contiene `pinCount` y `dirty`. `GetPage` incrementa el pin; `ReleasePage` lo reduce y marca cambios. Una página dirty se escribe al expulsarla o con `Flush`. |
| Índice principal B+ Tree | `BPlusTree` y `BPlusTreeNode`, en `src/index/bplus_tree.{h,cpp}`, mantienen nodos internos, hojas, claves, hijos y RIDs. |
| B+ Tree mediado por Buffer Manager | Cada acceso del árbol solicita un frame con `bufferManager_->GetPage(pageId)` y lo libera con `ReleasePage`. Los nodos se codifican en `storage::Page`; no se almacenan como nodos permanentes en una colección de RAM. |
| Modelo Volcano | La interfaz `Iterator` declara `open()`, `next()` y `close()` en `src/execution/iterator.h`. |
| Selección y proyección | `Select` filtra las tuplas producidas por su hijo; `Project` transforma cada tupla que recibe. |
| Algoritmo de join | `NestedLoopJoin` materializa la entrada interna y compara cada tupla externa con ella usando un predicado. |

## 4. Arquitectura y responsabilidades

### 4.1 Storage Manager y organización física de la página

`StorageManager` es responsable únicamente de la persistencia de páginas. La posición física de una página `pageId` se calcula como:

```text
offset_en_archivo = pageId × 4096 bytes
```

De esta manera, la página 0 ocupa los bytes `0..4095`, la página 1 los bytes `4096..8191`, y así sucesivamente. `GetNumPages()` se obtiene del tamaño real del archivo al abrirlo y se actualiza al escribir una página nueva.

La estructura lógica de una página de datos es de tipo *slotted page*:

```text
byte 0..1    numSlots          (uint16_t)
byte 2..3    freeSpacePointer  (uint16_t)
byte 4..5    slotDirectoryEnd  (uint16_t)
byte 6..     Slot[0], Slot[1], ...  cada Slot = {offset:uint16_t, size:uint16_t}
             espacio libre
             registros de tamaño variable, almacenados desde el final hacia atrás
byte 4095
```

Al insertar un registro, el directorio de slots crece desde el inicio y los bytes del registro se copian desde el final de la página hacia atrás. La inserción solo es válida si hay espacio tanto para el registro como para su entrada `Slot`. El RID resultante es `(pageId, slotId)` y permite localizar el registro sin buscarlo secuencialmente dentro de su página.

### 4.2 Buffer Manager

El buffer pool es una caché de páginas entre el almacenamiento y los consumidores superiores. Un `Frame` contiene:

```cpp
pageId, page, pinCount, dirty
```

Su protocolo de uso es:

1. El consumidor llama `GetPage(pageId)`; el frame se fija y `pinCount` aumenta.
2. Si la página ya estaba cargada, se registra un *hit*. Si no, se registra un *miss*, se lee desde el `StorageManager` y se instala en el pool.
3. El consumidor trabaja con `frame->page`.
4. Llama `ReleasePage(pageId, dirty)`. Si modificó la página, el flag `dirty` queda activo.
5. En una expulsión LRU o en `Flush()`, una página dirty se escribe de vuelta a disco.

La política LRU evita expulsar páginas con `pinCount > 0`; estas se encuentran en uso activo. La lista se interpreta como:

```text
frente de lruList_  ── menos recientemente usada
final de lruList_   ── más recientemente usada
```

Esta separación evita que un operador de ejecución tenga que manipular directamente archivos binarios y concentra la responsabilidad de caché, persistencia diferida y métricas de acceso en un único componente.

### 4.3 B+ Tree persistente

El B+ Tree implementa el índice principal. Su metadato se guarda en la página 0: un byte mágico (`0xB5`) y el identificador de página de la raíz. Esto permite reabrir la base de datos y recuperar la raíz sin reconstruir el índice en memoria.

Los nodos se serializan directamente en páginas de 4 KB. El encabezado del nodo contiene si es hoja, número de claves, padre y enlaces anterior/siguiente entre hojas. Después se almacenan claves y, según el tipo de nodo, RIDs (hoja) o IDs de hijos (interno).

```text
Página 0: metadatos del índice → rootPageId

Nodo interno: [cabecera | claves separadoras | childPageId...]
Nodo hoja:    [cabecera | claves | RID(pageId, slotId)... | next/prev]
```

La capacidad usada en las demostraciones es deliberadamente pequeña (`maxKeysLeaf = 3`, `maxKeysInternal = 3`) para provocar y mostrar *splits*, propagación de claves y cambios de raíz con pocos registros. No es la capacidad física máxima de una página de 4 KB.

Operaciones principales:

- `Insert(key, rid)`: crea la raíz inicial si es necesario, desciende hasta la hoja, inserta ordenadamente y divide nodos llenos. Una clave promovida puede generar divisiones recursivas hasta una nueva raíz.
- `Search(key)`: desciende desde `rootPageId_` comparando separadores hasta una hoja y devuelve el RID asociado o un RID inválido.
- `Delete(key)`: elimina en la hoja y, ante subocupación, intenta redistribuir con un hermano; de no ser posible, fusiona nodos y puede contraer la raíz.

Cada transición entre nodos usa `GetPage`/`ReleasePage`, por lo que incluso una búsqueda por índice participa de los hits, misses, pins y expulsiones del buffer pool.

### 4.4 Procesamiento de consultas: modelo Volcano

Todos los operadores implementan la misma interfaz:

```cpp
open();                 // inicializa estado y operadores hijos
next(out) -> bool;      // produce una tupla por llamada mientras haya resultados
close();                // libera frames y estado temporal
```

| Operador | Responsabilidad | Relación con el buffer |
|---|---|---|
| `Scan` | Recorre un rango de páginas y entrega sus registros uno por uno. | Fija la página actual, lee sus slots y la libera antes de pasar a la siguiente. |
| `Select` | Conserva solo las tuplas que cumplen un predicado. | Delega la lectura a su hijo; no toca páginas directamente. |
| `Project` | Aplica una función que devuelve solo los atributos requeridos. | Delega la lectura a su hijo. |
| `IndexScan` | Busca una clave exacta en el B+ Tree y luego lee el RID encontrado. | El árbol y la página de datos se solicitan al `BufferManager`. |
| `NestedLoopJoin` | Compara cada tupla externa con las tuplas de la entrada interna. | Sus dos hijos son iteradores; en esta versión materializa la entrada interna. |

La composición es posible porque el padre solamente conoce la interfaz del hijo. Por ejemplo, `Project` puede consumir `Select`, y `Select` puede consumir `Scan`, sin que alguno dependa de la implementación de los demás.

## 5. Ejemplo integral y orquestado

El ejecutable `full_flow_demo` (`src/execution/demo_full_flow.cpp`) demuestra el flujo completo. El escenario usa dos relaciones lógicas:

```text
Left : claves 1..10, en páginas desde la 1
Right: claves 5..14, en páginas desde la 20
```

### 5.1 Carga y persistencia

Para cada registro, la demo solicita la página de datos actual al buffer, inserta los bytes mediante el slot directory y libera el frame como dirty:

```text
"Left Key:5"
   │
GetPage(1) → Page::InsertRecord → slotId asignado
   │                                  │
ReleasePage(1, true)                  └→ RID = (1, slotId)
   │
Flush() → StorageManager::WritePageData → data/full_flow.db
```

El `Flush()` antes de construir el índice garantiza que las páginas de datos ya existan físicamente y que el árbol pueda reservar IDs nuevos sin colisionar con ellas.

### 5.2 Consulta secuencial compuesta

La demo construye el plan:

```text
Project( Select( Scan(Left) ) )
```

Equivalente conceptual:

```sql
SELECT parte_del_texto
FROM Left
WHERE representación_textual_de_la_clave contiene "Key:1" o "Key:2";
```

En los datos de la demo esto también incluye la clave 10, pues la condición se implementa como búsqueda de subcadena para mantener el ejemplo compacto.

La llamada `project.open()` se propaga hacia `Select` y luego hacia `Scan`. Cada llamada de `project.next(out)` desencadena la siguiente secuencia:

```text
Project.next
  → Select.next
      → Scan.next
          → BufferManager.GetPage(1)
          → Page.ReadAllRecords / entrega el siguiente slot
      → Select evalúa el predicado
  → Project transforma la tupla aprobada
→ resultado para el cliente
```

Al finalizar, `project.close()` se propaga hacia abajo y `Scan` libera, si existe, el frame que mantiene fijado.

### 5.3 Creación del índice y búsqueda exacta

La demo recorre los slots de `Right`, obtiene la clave de cada registro y registra `key → RID` en el B+ Tree. Para `Right Key:8`, el índice contiene un valor conceptual como:

```text
8 → RID(pageId=20, slotId=3)
```

El planificador usa una heurística sencilla: si hay B+ Tree y el predicado es una igualdad sobre la clave indexada, selecciona `IndexScan`.

```text
Predicado Key = 8
       │
Planner::UseIndexScan(...) = true
       │
IndexScan.open()
IndexScan.next()
   ├─ BPlusTree.Search(8)
   │    └─ GetPage(raíz) → ... → GetPage(hoja) → RID(20, 3)
   └─ GetPage(20) → ReadRecord(3) → "Right Key:8"
IndexScan.close()
```

Este ejemplo demuestra dos niveles de acceso: primero se consulta el índice persistente para recuperar el RID, y después se recupera la tupla real desde su página de datos. Ambos niveles pasan por el buffer pool.

### 5.4 Join entre las relaciones

El plan de join es:

```text
NestedLoopJoin(Scan(Left), Scan(Right), igualdad de clave)
```

Al abrirse, `NestedLoopJoin` materializa `Right`; después avanza una tupla de `Left` y la compara contra esa colección. Para las claves compartidas se producen seis pares:

```text
Left Key:5  | Right Key:5
Left Key:6  | Right Key:6
Left Key:7  | Right Key:7
Left Key:8  | Right Key:8
Left Key:9  | Right Key:9
Left Key:10 | Right Key:10
```

La complejidad temporal de esta estrategia es `O(|Left| × |Right|)` después de materializar la entrada interna. Es correcta y didáctica; para relaciones grandes sería preferible un `HashJoin`, un block nested-loop join o un index nested-loop join.

### 5.5 Demostración de LRU, `pinned` y `dirty`

El ejecutable principal `mini-sgbd` incluye una secuencia específica con un pool auxiliar de capacidad 5. Se solicitan páginas en un orden que supera la capacidad para forzar expulsiones. La página 2 se libera con `dirty=true`; si se convierte en víctima LRU, se escribe antes de salir del pool. Además, la página 0 se mantiene fijada mientras se piden otras páginas: LRU no puede elegirla como víctima hasta que se invoque `ReleasePage(0, false)`.

Esto permite sustentar que:

- una página dirty no se pierde al ser reemplazada, porque se persiste antes de expulsarla;
- una página pinned no se reemplaza mientras un consumidor la utiliza;
- el orden LRU se actualiza tanto con hits como con misses.

## 6. Resultados y validación

### 6.1 Pruebas funcionales disponibles

| Artefacto | Cobertura principal |
|---|---|
| `storage_test` | Inserción/lectura por slots y persistencia de una página en archivo binario. |
| `bplus_delete_test` | Búsqueda, eliminación, redistribución/fusión y contracción de raíz del B+ Tree. |
| `iterators_test` | Cadena `Scan → Select → Project`; espera dos resultados filtrados. |
| `gtest_iterators` | Seis casos: composición de operadores, heurística, selección, proyección, join e `IndexScan`. |
| `full_flow_demo` | Integración de carga, consulta Volcano, índice, planificador, join y métricas de buffer. |
| `buffer_benchmark` | Efecto del tamaño del pool en hits, misses y tiempo. |
| `benchmark_index_impact` | Comparación de una selección secuencial frente a `IndexScan`. |

La suite GoogleTest registra seis pruebas: `ScanSelectProject`, `UseIndexScanHeuristic`, `FilteredResult`, `TruncateProjection`, `JoinEquality` y `ExactKeySearch`.

### 6.2 Medición del Buffer Manager

El benchmark carga 2000 registros, ocupando páginas 1 a 9, y realiza cinco escaneos completos (45 accesos). Los resultados almacenados en `benchmark.csv` son:

| Buffer (frames) | Accesos | Hits | Misses | Hit rate | Tiempo (ms) |
|---:|---:|---:|---:|---:|---:|
| 1 | 45 | 0 | 45 | 0 % | 0.180 |
| 2 | 45 | 0 | 45 | 0 % | 0.188 |
| 5 | 45 | 0 | 45 | 0 % | 0.173 |
| 10 | 45 | 36 | 9 | 80 % | 0.063 |
| 20 | 45 | 36 | 9 | 80 % | 0.054 |
| 50 | 45 | 36 | 9 | 80 % | 0.054 |

Interpretación: el conjunto de trabajo tiene nueve páginas. Con menos de nueve frames, cada escaneo secuencial vuelve a expulsar páginas que serán necesarias en el siguiente ciclo, por lo que no hay reutilización. Con 10 o más frames, el primer recorrido provoca nueve misses y los cuatro recorridos restantes se resuelven desde memoria: `4 × 9 = 36` hits. La gráfica está disponible en `docs/figures/buffer_hitrate.svg`.

### 6.3 Comparación Scan versus IndexScan

El benchmark de índice usa 100 registros que, por su tamaño pequeño, caben en una sola página de datos. Por tanto, los valores no deben interpretarse como una comparación de I/O para una tabla grande; sirven para verificar el camino funcional y el costo de navegación del árbol.

| Clave | Scan: accesos/misses | IndexScan: accesos/misses | Scan (ms) | Índice (ms) |
|---:|---:|---:|---:|---:|
| 5 | 1 / 0 | 6 / 4 | 0.079 | 0.027 |
| 25 | 1 / 0 | 6 / 3 | 0.074 | 0.019 |
| 50 | 1 / 0 | 6 / 3 | 0.091 | 0.020 |
| 75 | 1 / 0 | 6 / 2 | 0.069 | 0.014 |
| 99 | 1 / 0 | 6 / 0 | 0.070 | 0.006 |

El índice realiza más accesos porque atraviesa niveles del B+ Tree y luego la página de datos, mientras que el escaneo toca una sola página ya caliente. En un conjunto de una sola página, el índice no ofrece ventaja de I/O; su utilidad se aprecia cuando la tabla ocupa muchas páginas o cuando se reutilizan nodos del índice. El tiempo observado favorece al `IndexScan` en esta ejecución local, pero no es una conclusión general de rendimiento: los tiempos son submilisegundo y dependen de la caché y del entorno. Esta lectura crítica evita sobreafirmar los resultados.

## 7. Cómo reproducir la demostración

Desde la raíz del repositorio:

```bash
cmake -S . -B build
cmake --build build

(cd build && ./storage_test)
./build/bplus_delete_test
./build/gtest_iterators
./build/demostracion_practica
./build/full_flow_demo
./build/buffer_benchmark
./build/benchmark_index_impact
./build/mini-sgbd
```

Si el proyecto ya fue configurado, basta con ejecutar `cmake --build build` y los binarios necesarios. `storage_test` usa la ruta relativa `../data/test_db.bin`, por eso se ejecuta desde `build`; los demás comandos se ejecutan desde la raíz del repositorio.

Para una sustentación breve se recomienda ejecutar primero:

```bash
./build/demostracion_practica
./build/full_flow_demo
./build/mini-sgbd
```

`demostracion_practica` es la demostración guiada: imprime cada inserción, los slots y RIDs creados, los estados dirty/pinned, la estructura del índice y el flujo de cada operador. `full_flow_demo` muestra el flujo integrado de consultas; `mini-sgbd` hace visible una carga mayor, la persistencia del B+ Tree y una demostración adicional de LRU/pin/dirty.

## 8. Guion sugerido de sustentación

1. **Problema.** “Un SGBD necesita conservar datos, administrar una memoria limitada, localizar registros eficientemente y ejecutar consultas como un plan.”
2. **Página y disco.** Mostrar que cada archivo se divide en bloques de 4096 bytes y explicar el slot directory: los slots apuntan a registros de tamaño variable mediante offset y tamaño.
3. **Buffer.** Explicar `GetPage`/`ReleasePage`; señalar que pinned protege una página en uso y dirty indica que debe persistirse. Mostrar LRU en `mini-sgbd`.
4. **Índice.** Explicar que el B+ Tree no vive solo en RAM: cada nodo es una página, la página 0 conserva la raíz y una búsqueda retorna un RID.
5. **Consulta.** Presentar `Scan → Select → Project` y el protocolo `open/next/close`. Resaltar que cada operador tiene una responsabilidad única.
6. **Búsqueda indexada.** Mostrar `Key = 8`: planificador → `IndexScan` → B+ Tree → RID → página de datos.
7. **Join.** Mostrar los seis pares de claves comunes y mencionar el costo de Nested Loop Join.
8. **Evidencia.** Presentar el benchmark: nueve páginas de trabajo, 0 % de hit rate con pool menor a nueve y 80 % cuando el pool las puede retener.
9. **Cierre.** “La solución integra persistencia, caché, índice y ejecución; no son módulos aislados.”

## 9. Limitaciones conocidas y trabajo futuro

El informe distingue las funcionalidades implementadas de las mejoras necesarias para un motor más robusto:

- Los registros de los ejemplos se serializan como texto; faltan esquema, tipos, catálogo y serialización de columnas formal.
- `Scan` recibe explícitamente un rango de páginas; aún no existe un catálogo de tablas que conozca sus páginas de forma automática.
- El planificador usa una heurística binaria: elige `IndexScan` para igualdad si existe índice. No estima cardinalidad, selectividad ni costos.
- `NestedLoopJoin` materializa toda la entrada interna y tiene costo cuadrático; no es apropiado para relaciones grandes.
- El B+ Tree ofrece búsqueda exacta; un siguiente paso natural es implementar escaneos por rango aprovechando el enlace entre hojas.
- No hay transacciones, WAL, recuperación ante fallos, control de concurrencia ni bloqueo de páginas; por ello el sistema debe entenderse como monohilo y educativo.
- La gestión de pins presupone que todos los consumidores liberan correctamente cada frame. En una evolución de producción, si el pool está lleno y todas las páginas están pinned, `GetPage` debería devolver un error o esperar, en vez de intentar continuar sin una víctima disponible.
- La medición de impacto del índice debe repetirse con registros suficientemente grandes para ocupar múltiples páginas, buffer frío y varias repeticiones; eso permitirá separar el costo de CPU del costo real de I/O.

## 10. Conclusión

El Mini SGBD cumple las consideraciones técnicas solicitadas y, sobre todo, demuestra su integración. El registro se almacena en una página binaria con slot directory; la página se administra en el buffer pool con LRU, pins y páginas dirty; el B+ Tree guarda RIDs persistentes en páginas recuperadas por el buffer; y los operadores Volcano componen lecturas, filtros, proyecciones, búsquedas indexadas y joins.

La principal evidencia de diseño es que una consulta no salta estas capas: para obtener una clave indexada, el sistema navega nodos B+ persistentes, recupera un RID y lee el registro físico a través del mismo `BufferManager`. Esta coordinación representa el comportamiento esencial de un gestor de base de datos a escala reducida y deja una base clara para incorporar optimización, concurrencia y recuperación en etapas posteriores.
