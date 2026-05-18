@echo off
echo =========================================
echo Compilando pruebas de almacenamiento...
echo =========================================

cmake -S . -B build
cmake --build build

echo.
echo =========================================
echo Ejecutando pruebas de almacenamiento...
echo =========================================
echo.

cd build
cd Debug 2>nul || cd .
if exist storage_test.exe (
    storage_test.exe
) else (
    echo Error: No se encontro el ejecutable storage_test.exe
)
cd ..\..
echo.
pause
