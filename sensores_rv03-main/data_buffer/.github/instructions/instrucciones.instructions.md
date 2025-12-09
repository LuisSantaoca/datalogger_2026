# Reglas de Programación para Arduino

## 1. Estándares Generales de Código
- Utilizar indentación consistente de 2 o 4 espacios.
- No escribir comentarios dentro de las funciones.
- Las documentcion debe ser tipo Doxygen.
- en los archivos .h documenta cada funcion y variable
- Evitar líneas de más de 80–100 caracteres.
- Mantener funciones pequeñas y con una sola responsabilidad.

## 2. Nombres de Variables y Funciones
- Usar nombres descriptivos.
- Variables en `camelCase`.
- Constantes en `MAYÚSCULAS_CON_GUIONES`.
- Funciones en `camelCase` iniciando con verbos.
- Evitar abreviaciones no obvias.

## 3. Uso Eficiente de Memoria
- Usar tipos de datos pequeños cuando sea posible (`byte`, `bool`).
- Declarar variables globales solo cuando sea necesario.
- Evitar el uso de memoria dinámica.

## 4. Organización de Archivos
- Separar código en `.h` y `.cpp` para proyectos grandes.
- Crear un archivo `config.h` para constantes globales.
- En el archivo .ino solo la lógica para probar las funciones.