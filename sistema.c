#include "ext4.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/vfs.h>
#include <ctype.h>
#include <curses.h>

void initScreen(){
    initscr();
	raw(); 
	keypad(stdscr, TRUE);	/* Para obtener F1,F2,.. */
	noecho();
    curs_set(0); 
}

int seleccionaParticion(int numParticiones, unsigned int *sectoresInicio){
    int seleccion = 0;
    int continuar = 1; // Variable de control para el bucle

    while (continuar) {
        clear(); // Limpiar la pantalla
        printw("Selecciona una partición\n\n");
        for (int i = 0; i < numParticiones; i++) {
            if (i == seleccion) {
                attron(A_REVERSE); // Resaltar la selección actual
            }
            printw("Partición %d: Sector de inicio %u\n\n", i + 1, sectoresInicio[i]);
            if (i == seleccion) {
                attroff(A_REVERSE);
            }
        }
        refresh(); // Actualizar la pantalla

        int ch = getch(); // Obtener la tecla presionada
        switch (ch) {
            case KEY_UP:
                if (seleccion > 0) seleccion--;
                break;
            case KEY_DOWN:
                if (seleccion < numParticiones - 1) seleccion++;
                break;
            case 10: // Tecla Enter
                continuar = 0; // Cambiar la variable de control para salir del bucle
                break;
        }
    }
    clear();
    printw("Has seleccionado la partición %d \n\n", seleccion + 1);
    printw("Presiona una tecla para continuar...");
    refresh();
    getchar();
    clear();
    return sectoresInicio[seleccion];
}

int leerParticion(int fd){
    unsigned int *sectoresInicio = NULL;
    // Leer los primeros 512 bytes (tamaño de un sector)
    unsigned char mbr[512];
    ssize_t bytesLeidos = read(fd, mbr, sizeof(mbr));
    if (bytesLeidos == -1) {
        perror("Error al leer el MBR");
        close(fd);
        return 1;
    }

    // Asegurarse de que se leyeron los 512 bytes completos
    if (bytesLeidos != sizeof(mbr)) {
        fprintf(stderr, "No se leyeron los 512 bytes completos del MBR\n");
        close(fd);
        return 1;
    }

    // La tabla de particiones comienza en el byte 446 del MBR
    int partitionTableStart = 446;
    // Cada entrada de la tabla de particiones tiene 16 bytes
    int partitionEntrySize = 16;
    // La dirección de inicio de la partición está en los bytes 8-11 de cada entrada
    int startAddressOffset = 8;

    int numParticiones = 0;

    // Iterar a través de las cuatro entradas de la tabla de particiones
    for (int i = 0; i < 4; i++) {
        int entryStart = partitionTableStart + (i * partitionEntrySize);
        unsigned int startSector = *(unsigned int *)&mbr[entryStart + startAddressOffset];

        // Ignorar entradas de partición no utilizadas
        if (startSector != 0) {
             // Reasignar memoria para el nuevo tamaño del arreglo
            unsigned int *temp = realloc(sectoresInicio, (numParticiones + 1) * sizeof(unsigned int));
            if (temp == NULL) {
                fprintf(stderr, "Error al asignar memoria.\n");
                free(sectoresInicio);
                return 1;
            }
            sectoresInicio = temp;

            // Almacenar el sector de inicio
            sectoresInicio[numParticiones] = startSector;
            numParticiones++;
        }
    }
    int seleccion = 0;
    if(sectoresInicio != NULL){
        printw("Seleccionar particion\n\n");
        seleccion = seleccionaParticion(numParticiones, sectoresInicio);
    }else{
        return -1;
    }


    return seleccion;
}


int main() {

    initScreen();

    int direccion_particion;
    
    int fd = open("imagen.img", O_RDONLY);
    if (fd < 0) {
        perror("Error al abrir la imagen del sistema de archivos");
        endwin();
        return 1;
    }

    direccion_particion = leerParticion(fd);

    if(direccion_particion == -1){
        printw("Error. No se encontró ninguna particion\n\n");
        printw("Presiona una tecla para continuar...");
        refresh();
        getchar();
        endwin();
        close(fd);
        return 1;
    } 

    printw("La partición comienza en el sector: %u\n", direccion_particion);

    refresh();

    getchar();

    endwin();

    close(fd);

    return 0;
}
