@echo off
echo =========================================
echo Compilando mini-SGBD y pruebas...
echo =========================================

cmake -S . -B build
if %ERRORLEVEL% NEQ 0 (
    echo Error en la configuracion de CMake.
    pause
    exit /b %ERRORLEVEL%
)

cmake --build build
if %ERRORLEVEL% NEQ 0 (
    echo Error en la compilacion de los archivos.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo =========================================
echo Ejecutando bateria de pruebas...
echo =========================================
echo.

cd build
cd Debug 2>nul || cd .

set TESTS=test_bplus_insert.exe test_bplus_delete.exe test_bplus_search.exe test_buffer_manager.exe test_persistence.exe

for %%T in (%TESTS%) do (
    if exist %%T (
        echo.
        echo -----------------------------------------
        echo Ejecutando %%T...
        echo -----------------------------------------
        %%T
        if %ERRORLEVEL% NEQ 0 (
            echo [FALLO] %%T termino con error.
            cd ..\..
            pause
            exit /b %ERRORLEVEL%
        )
    ) else (
        echo [ERROR] No se encontro el ejecutable %%T
    )
)

cd ..\..
echo.
echo =========================================
echo Todas las pruebas pasaron exitosamente!
echo =========================================
echo.
pause
