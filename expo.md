# Guion de exposición: Modelo Volcano y operadores de ejecución

## 1. Introducción

Buenas tardes. Hoy voy a hablar sobre la parte del proyecto que implementa el modelo Volcano, también conocido como modelo de iteradores, y sobre cómo se organizan los operadores de ejecución en este motor.

La idea principal es simple: cada operador funciona como un "iterador" que produce datos uno por uno, de manera modular. Esto permite construir planes de ejecución más flexibles, donde un operador puede recibir datos de otro operador y transformarlos paso a paso.

## 2. Qué es el modelo Volcano (Iterator)

En este proyecto, la base del modelo está en el archivo [src/execution/iterator.h](src/execution/iterator.h).

Aquí se define una interfaz común para todos los operadores mediante tres métodos:

- open(): inicializa el operador.
- next(): devuelve el siguiente registro o tupla.
- close(): libera recursos y finaliza la ejecución.

Este diseño es importante porque permite que todos los operadores compartan la misma forma de trabajo. Por ejemplo, un operador de selección, uno de proyección o uno de escaneo pueden ser tratados de manera uniforme aunque hagan tareas distintas.

### Ejemplo de idea central

Podemos pensar en cada operador como una máquina que recibe datos y produce datos:

- un escáner lee registros desde una tabla,
- un selector filtra algunos,
- un proyector elige solo ciertas columnas.

Todo esto se une como una cadena de iteradores.

## 3. ScanOperator / Scan

El operador de escaneo está implementado en [src/execution/scan.h](src/execution/scan.h) y [src/execution/scan.cpp](src/execution/scan.cpp).

Su función es recorrer secuencialmente los registros de una tabla usando el BufferManager. En otras palabras, toma páginas de datos y va devolviendo un registro por cada llamada a next().

### Qué hace en el código

En el archivo de implementación se observa que:

- se inicializa la página actual,
- se leen los registros de la página,
- se devuelve uno por uno,
- cuando una página se agota, pasa a la siguiente.

### Ejemplo para explicar

Podemos decir algo como:

> "Si la tabla tiene 100 registros, el operador Scan no los devuelve todos de una vez, sino que los entrega uno a uno, tal como un cursor que avanza sobre la tabla."

### Punto clave para mencionar

Este operador es la base de muchos planes de ejecución, porque suele ser el primero en la cadena.

## 4. SelectOperator / Select

El operador de selección está en [src/execution/select.h](src/execution/select.h) y [src/execution/select.cpp](src/execution/select.cpp).

Su función es filtrar los registros que llegan de otro operador. Recibe un predicado, es decir, una condición, y devuelve únicamente los datos que cumplen con esa condición.

### Qué hace en el código

En la implementación, el operador:

- llama a next() del operador hijo,
- revisa si el registro cumple la condición,
- si la cumple, lo devuelve,
- si no, sigue buscando en los siguientes registros.

### Ejemplo para explicar

Podemos decir:

> "Si tenemos una tabla de estudiantes y queremos solo los que tienen edad mayor a 18, el operador Select aplica esa condición y deja pasar solamente los registros válidos."

### Punto clave para mencionar

Es un operador de filtrado. No modifica los datos, solo decide qué registros siguen adelante.

## 5. ProjectOperator / Project

El operador de proyección está en [src/execution/project.h](src/execution/project.h) y [src/execution/project.cpp](src/execution/project.cpp).

Este operador devuelve únicamente las columnas que solicita la consulta. En lugar de entregar todo el registro completo, toma solo las partes requeridas.

### Qué hace en el código

En la implementación:

- recibe un registro del operador hijo,
- aplica una función de proyección,
- devuelve una versión más pequeña del registro con solo las columnas deseadas.

### Ejemplo para explicar

Podemos decir:

> "Si el registro tiene nombre, edad y correo, pero la consulta solo necesita nombre y correo, el operador Project elimina la columna edad y deja solo lo necesario."

### Punto clave para mencionar

Es un operador de transformación de columnas. Reduce la cantidad de datos que se pasan a la siguiente etapa.

## 6. Cómo se relacionan entre sí

Una manera sencilla de explicar la arquitectura es mostrar esta idea:

1. Scan lee los datos de la tabla.
2. Select filtra los registros que cumplen una condición.
3. Project deja solo las columnas necesarias.

Así, el flujo se organiza como una cadena de operadores, cada uno con una tarea específica.

## 7. Conclusión

En resumen, el proyecto implementa un motor de ejecución basado en iteradores, donde cada operador sigue una interfaz común y puede conectarse con otros operadores para formar un plan de consulta.

Lo más importante que quiero destacar es que esta arquitectura permite:

- reutilizar componentes,
- separar responsabilidades,
- construir consultas de manera modular,
- hacer que el código sea más claro y extensible.

## 8. Cierre sugerido para la exposición

Gracias. Como se puede ver, el modelo Volcano facilita la ejecución de consultas de forma organizada, permitiendo que cada operador haga una tarea concreta y que todos trabajen juntos como una cadena de procesamiento.
