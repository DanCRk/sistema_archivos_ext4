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

int leerSuperBloque(int fd,int inicioParticion){

    // Lee el superbloque
    int inicio_superbloque = (inicioParticion * 512) + 0x400;
    struct ext4_super_block sb;
    if (pread(fd, &sb, sizeof(sb), inicio_superbloque) != sizeof(sb)) {
        printw("Error leyendo el superbloque");
        refresh();
        close(fd);
        endwin();
        return;
    }

    // Calcula el tamaño de bloque y el tamaño de inode
    size_t tamano_bloque = 1024 << sb.s_log_block_size;
    size_t tamano_inode = 256 << sb.s_inode_size; // Corrección aquí

    // Imprime la información del superbloque
    printw("Superblock\n\n");
    printw("Cuenta de Inodes: %u\n\n", sb.s_inodes_count);
    printw("Cuenta de Blocks: %u\n\n", sb.s_blocks_count_lo);
    printw("Etiqueta Disco: %.*s\n\n", EXT4_LABEL_MAX, sb.s_volume_name);
    printw("Primer inode: %u\n\n", sb.s_first_ino);
    printw("Block size: %zu\n\n", tamano_bloque);
    printw("Inode size: %zu\n\n", tamano_inode); // Corrección aquí
    printw("Blocks per group: %u\n\n", sb.s_blocks_per_group);
    printw("Inodes per group: %u\n\n", sb.s_inodes_per_group);
    printw("Presiona una tecla para continuar...");
    refresh();
    getchar();
    clear();
    return inicio_superbloque;
}

int main() {

    initScreen();

    int sectorInicioParticion;
    
    int fd = open("imagen.img", O_RDONLY);
    if (fd < 0) {
        perror("Error al abrir la imagen del sistema de archivos");
        endwin();
        return 1;
    }

    sectorInicioParticion = leerParticion(fd);

    if(sectorInicioParticion == -1){
        printw("Error. No se encontró ninguna particion\n\n");
        printw("Presiona una tecla para continuar...");
        refresh();
        getchar();
        endwin();
        close(fd);
        return 1;
    } 

    int inicio_superbloque = leerSuperBloque(fd,sectorInicioParticion);

    refresh();

    getchar();

    endwin();

    close(fd);

    return 0;
}
