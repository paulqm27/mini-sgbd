# Explicación simple de la eliminación en el B+ Tree

Este documento resume de forma sencilla cómo funciona la eliminación en el árbol B+ del proyecto, y cómo se integra con el Buffer Manager y la persistencia.

## 1. Qué hace la eliminación

La eliminación borra una clave de una hoja del árbol y luego revisa si el nodo quedó con muy pocas claves. Si ocurre eso, el árbol intenta:

- redistribuir claves con un nodo vecino, o
- fusionar nodos si no hay suficiente espacio.

El objetivo es mantener el árbol balanceado y que siga funcionando correctamente.

## 2. Punto clave: la función Delete

La función principal es:

```cpp
bool BPlusTree::Delete(int key)
```

Su trabajo es:

1. Buscar la clave en la hoja correcta.
2. Eliminarla del nodo hoja.
3. Revisar si la hoja quedó vacía o con muy pocas claves.
4. Si hace falta, llamar a la lógica de reequilibrio.

## 3. Reequilibrio: underflow

Cuando un nodo queda con menos claves de las permitidas, se produce un underflow. Entonces se llama a:

```cpp
bool BPlusTree::HandleUnderflow(int pageId)
```

Esta función:

- identifica el padre del nodo,
- encuentra un nodo hermano,
- intenta redistribuir o fusionar,
- y mantiene la estructura del árbol consistente.

## 4. Redistribución

Si el nodo hermano tiene suficientes claves, se mueve una clave al nodo que quedó pequeño:

```cpp
bool BPlusTree::RedistributeLeafNodes(...)
bool BPlusTree::RedistributeInternalNodes(...)
```

Esto evita hacer una fusión innecesaria.

## 5. Fusión

Si no se puede redistribuir, entonces se fusionan los nodos:

```cpp

void BPlusTree::MergeLeafNodes(...)
void BPlusTree::MergeInternalNodes(...)


```

En este caso:

- se combinan las claves de ambos nodos,
- se elimina la entrada del padre que separaba a ambos,
- y en algunos casos se reduce el árbol si la raíz queda vacía.

## 6. Integración con el Buffer Manager

El árbol no trabaja directamente con disco en cada operación. Usa el Buffer Manager para cargar y guardar páginas en memoria.

Ejemplo de uso:

```cpp
buffer::Frame* frame = bufferManager_->GetPage(pageId);
BPlusTreeNode node(frame->page.get());
```

Luego, cuando se modifica una página, se marca como dirty para que se escriba al final:

```cpp
bufferManager_->ReleasePage(pageId, true);
```

Esto permite que la eliminación sea eficiente y que el árbol use caché en memoria.

## 7. Integración con la persistencia

Cuando se hace una modificación importante, como una eliminación que cambia la estructura del árbol, los cambios quedan reflejados en las páginas del almacenamiento. Luego, al recargar el árbol, se puede reconstruir desde disco.

El flujo es:

1. se modifica el árbol en memoria,
2. el Buffer Manager escribe las páginas dirty,
3. al reiniciar, el árbol vuelve a cargar su estado desde disco.

## 8. En resumen

La eliminación en el B+ Tree es una combinación de:

- borrar la clave en la hoja,
- revisar si el nodo quedó pequeño,
- redistribuir si se puede,
- fusionar si es necesario,
- y dejar todo consistente para que siga funcionando con Buffer Manager y persistencia.

Este proceso es clave porque permite que el árbol siga siendo balanceado y eficiente incluso después de muchas eliminaciones.
