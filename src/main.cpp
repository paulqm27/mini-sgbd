#include <iostream>

int main() {
    std::cout << "=========================================================" << std::endl;
    std::cout << "                MINI SGBD (C++)                          " << std::endl;
    std::cout << "=========================================================" << std::endl;
    std::cout << "Este es el punto de entrada principal del motor de base de datos." << std::endl;
    std::cout << "Para ejecutar las pruebas del sistema, compile y ejecute" << std::endl;
    std::cout << "los archivos correspondientes en la carpeta /tests." << std::endl;
    std::cout << "\nPruebas disponibles:" << std::endl;
    std::cout << "  - test_bplus_insert: Pruebas de insercion y splits" << std::endl;
    std::cout << "  - test_bplus_delete: Pruebas de eliminacion y fusiones" << std::endl;
    std::cout << "  - test_bplus_search: Pruebas de busqueda en el indice" << std::endl;
    std::cout << "  - test_buffer_manager: Pruebas de politicas LRU y pins" << std::endl;
    std::cout << "  - test_persistence: Pruebas de consistencia y recarga de disco" << std::endl;
    std::cout << "=========================================================" << std::endl;
    return 0;
}
