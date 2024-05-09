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
#include <string.h>
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
        return 1;
    }

    // Asegurarse de que se leyeron los 512 bytes completos
    if (bytesLeidos != sizeof(mbr)) {
        fprintf(stderr, "No se leyeron los 512 bytes completos del MBR\n");
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

    free(sectoresInicio);


    return seleccion;
}

int leerSuperBloque(int fd,int inicioParticion){

    // Lee el superbloque
    int inicio_superbloque = inicioParticion + 0x400;
    struct ext4_super_block sb;
    if (pread(fd, &sb, sizeof(sb), inicio_superbloque) != sizeof(sb)) {
        printw("Error leyendo el superbloque");
        refresh();
        endwin();
        return -1;
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

int leerDescriptorBloques(int fd,int inicio_superbloque){
    int inicio_descriptor_bloques = inicio_superbloque + 0x400;
    // Lee el descriptor de bloques
    struct ext4_group_desc descriptor_bloques;
    if (pread(fd, &descriptor_bloques, sizeof(descriptor_bloques), inicio_descriptor_bloques) != sizeof(descriptor_bloques)) {
        perror("Error leyendo el superbloque");
        return -1;
    }
    // Imprime la información del descriptor de bloques
    printw("Descriptor de Bloques\n\n");
    printw("Dirección del bloque de bitmap de bloques: %u\n\n", descriptor_bloques.bg_block_bitmap_lo);
    printw("Dirección del bloque de bitmap de inodes: %u\n\n", descriptor_bloques.bg_inode_bitmap_lo);
    printw("Dirección del primer bloque de la tabla de inodes: %u\n\n", descriptor_bloques.bg_inode_table_lo);
    printw("Número de bloques libres: %u\n\n", descriptor_bloques.bg_free_blocks_count_lo);
    printw("Número de inodes libres: %u\n\n", descriptor_bloques.bg_free_inodes_count_lo);
    printw("Número de directorios: %u\n\n", descriptor_bloques.bg_used_dirs_count_lo);
    printw("Flags del grupo: %u\n\n", descriptor_bloques.bg_flags);
    printw("Checksum del descriptor: %u\n\n", descriptor_bloques.bg_checksum);
    printw("Presiona una tecla para continuar...");
    refresh();
    getchar();
    clear();
    return descriptor_bloques.bg_inode_table_lo;
}

struct ext4_dir_entry_2** leerBloque(int fd, int inicioBloque, int *numEntradas) {
    *numEntradas = 0; // Inicializar el contador de entradas
    struct ext4_dir_entry_2 **entradas = NULL; // Puntero para el arreglo de apuntadores
    int offset = 0;
    struct ext4_dir_entry_2 entrada;

    while (offset < 1024) {
        if (pread(fd, &entrada, sizeof(entrada), inicioBloque + offset) != sizeof(entrada)) {
            perror("Error leyendo el bloque de directorio");
            // Liberar la memoria si hay un error
            for (int i = 0; i < *numEntradas; i++) {
                free(entradas[i]);
            }
            free(entradas);
            return NULL;
        }

        if (entrada.rec_len == 0 || entrada.name_len == 0) {
            break;
        }

        // Reasignar memoria para agregar un nuevo apuntador
        struct ext4_dir_entry_2 **temp = realloc(entradas, (*numEntradas + 1) * sizeof(struct ext4_dir_entry_2*));
        if (temp == NULL) {
            perror("Error al reasignar memoria para entradas");
            // Liberar la memoria si hay un error
            for (int i = 0; i < *numEntradas; i++) {
                free(entradas[i]);
            }
            free(entradas);
            return NULL;
        }
        entradas = temp;

        // Crear una copia de la entrada y almacenar el apuntador en el arreglo
        entradas[*numEntradas] = malloc(sizeof(struct ext4_dir_entry_2));
        if (entradas[*numEntradas] == NULL) {
            perror("Error al asignar memoria para una entrada");
            // Liberar la memoria si hay un error
            for (int i = 0; i <= *numEntradas; i++) {
                free(entradas[i]);
            }
            free(entradas);
            return NULL;
        }
        *entradas[*numEntradas] = entrada;
        (*numEntradas)++;

        offset += entrada.rec_len;
    }

    return entradas; // Retornar el arreglo de apuntadores
}

int leerInodes(int fd, int inicioInode, int inicioParticion){
    clear(); // Limpiar la pantalla
    struct ext4_inode inode;
    if (pread(fd, &inode, sizeof(inode), inicioInode) != sizeof(inode)) {
        perror("Error leyendo el inode");
        printw("Presiona una tecla para continuar...");
        refresh();
        getchar();
        endwin();
        clear();
        return -1;
    }
    struct ext4_dir_entry_2** entradas = NULL; // Lista de todas las entradas
    int numEntradas = 0; // Contador de entradas
    // Itera sobre los punteros directos
    // Itera sobre los punteros directos
    for (int i = 0; i < 12; i++) {
        if (inode.i_block[i] != 0) { // Verifica que el puntero no sea nulo
            int direccion_bloque = (inode.i_block[i] * 0x400) + inicioParticion;
            int numEntradasBloque = 0; // Contador de entradas por bloque
            struct ext4_dir_entry_2** entradasBloque = leerBloque(fd, direccion_bloque, &numEntradasBloque);
            if (entradasBloque == NULL) {
                continue; // Si leerBloque falla, continúa con el siguiente bloque
            }
            // Añadir las entradas del bloque a la lista de todas las entradas
            for (int j = 0; j < numEntradasBloque; j++) {
                entradas = realloc(entradas, (numEntradas + 1) * sizeof(struct ext4_dir_entry_2*));
                if (entradas == NULL) {
                    perror("Error al reasignar memoria para las entradas");
                    endwin();
                    return -1;
                }
                entradas[numEntradas] = entradasBloque[j];
                numEntradas++;
                // Imprime cada entrada
                printw("%.*s -> %u\n", entradas[numEntradas - 1]->name_len, entradas[numEntradas - 1]->name, entradas[numEntradas - 1]->inode);
            }
            free(entradasBloque); // Liberar la memoria del bloque actual
        }
    }

    int seleccion = 0; // Índice de la entrada seleccionada
    int continuar = 1; // Variable de control para el bucle de selección

    // Bucle de selección
    while (continuar) {
        clear(); // Limpiar la pantalla
        printw("Directorio \n\n");
        for (int i = 0; i < numEntradas; i++) {
            if (i == seleccion) {
                attron(A_REVERSE); // Resaltar la selección actual
            }
            printw("%.*s -> %u\n", entradas[i]->name_len, entradas[i]->name, entradas[i]->inode);
            if (i == seleccion) {
                attroff(A_REVERSE);
            }
        }
        refresh(); // Actualizar la pantalla

        int ch = getch(); // Obtener la tecla presionada
        if (ch == 3) { // Ctrl+C
            continuar = 0; // Cambiar la variable de control para salir del bucle
            // No olvides liberar la memoria de las entradas antes de finalizar
        for (int i = 0; i < numEntradas; i++) {
            free(entradas[i]);
        }
        free(entradas);
            return -1;
        }
        
        switch (ch) {
            case KEY_UP:
                if (seleccion > 0) seleccion--;
                break;
            case KEY_DOWN:
                if (seleccion < numEntradas - 1) seleccion++;
                break;
            case 10: // Tecla Enter
                continuar = 0; // Cambiar la variable de control para salir del bucle
                break;
        }
    }

    // Aquí puedes hacer algo con el inode seleccionado, como retornarlo
    unsigned int inodeSeleccionado = entradas[seleccion]->inode;
    for (int i = 0; i < numEntradas; i++) {
        free(entradas[i]);
    }
    free(entradas);
    clear();

    return inodeSeleccionado; // Retorna un valor indicando éxito
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Error: No se ha proporcionado la ruta de la imagen\n");
        return 1;
    }

    initScreen();

    int inicioParticion;
    
    int fd = open((char *)argv[1], O_RDONLY);
    if (fd < 0) {
        perror("Error al abrir la imagen del sistema de archivos");
        endwin();
        return 1;
    }

    inicioParticion = leerParticion(fd) * 512;

    if(inicioParticion == -1){
        printw("Error. No se encontró ninguna particion\n\n");
        printw("Presiona una tecla para continuar...");
        refresh();
        getchar();
        endwin();
        return 1;
    } 

    int inicio_superbloque = leerSuperBloque(fd,inicioParticion);

    int inicio_tabla_inodes = leerDescriptorBloques(fd,inicio_superbloque);

    long inicioInodes = (inicio_tabla_inodes * 0x400) + inicioParticion + 256;
    long numeroInode = leerInodes(fd,inicioInodes,inicioParticion);

    while(numeroInode >0){
        printw("Inode seleccionado: %ld\n\n",numeroInode);

        inicioInodes = (inicio_tabla_inodes * 0x400) + inicioParticion + (256 * (numeroInode - 1));
        printw("Inode hex: %lX\n\n",inicioInodes);

        refresh();
        getchar();
        numeroInode = leerInodes(fd,inicioInodes, inicioParticion);
    }
    
    endwin();

    close(fd);

    return 0;
}
