#include "ext4.h"
#include "hexEditor.h"
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

// Estructura para almacenar los detalles de una partición
typedef struct {
    unsigned int startSector;
    unsigned int chs;
    unsigned int tipo;
    unsigned int lba;
    unsigned int tam;
} Particion;


void initScreen(){
    initscr();
    raw();
	keypad(stdscr, TRUE);	/* Para obtener F1,F2,.. */
    noecho();
    curs_set(0);
}

void mostrarBarraDeEstadoDir() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x); // Obtiene las dimensiones de la ventana
    mvprintw(max_y - 1, 0, "CRTL + C: Salir | flehca arriba: Subir | flehca abajo: Bajar | Enter: Seleccionar"); // Muestra los comandos
    clrtoeol(); // Limpia el resto de la línea
}



int seleccionaParticion(int numParticiones, Particion *particiones){
    int seleccion = 0;
    int continuar = 1; // Variable de control para el bucle

    while (continuar) {
        clear(); // Limpiar la pantalla
        printw("Particion\t\tCHS\t\tTipo\t\tLBA\t\tTAM\n\n");
        for (int i = 0; i < numParticiones; i++) {
            if (i == seleccion) {
                attron(A_REVERSE); // Resaltar la selección actual
            }
            printw("%d\t\t%d\t\t%d\t\t%d\t\t%d\n", i, particiones[i].chs, particiones[i].tipo, particiones[i].lba, particiones[i].tam);
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
    return particiones[seleccion].lba;
}

int leerParticion(int fd) {
    Particion *particiones = NULL;
    unsigned char mbr[512];
    ssize_t bytesLeidos = read(fd, mbr, sizeof(mbr));
    if (bytesLeidos == -1) {
        perror("Error al leer el MBR");
        return 1;
    }
    if (bytesLeidos != sizeof(mbr)) {
        fprintf(stderr, "No se leyeron los 512 bytes completos del MBR\n");
        return 1;
    }

    int partitionTableStart = 446;
    int partitionEntrySize = 16;
    int startAddressOffset = 8;
    int numParticiones = 0;

    for (int i = 0; i < 4; i++) {
        int entryStart = partitionTableStart + (i * partitionEntrySize);
        unsigned int startSector = *(unsigned int *)&mbr[entryStart + startAddressOffset];
        unsigned int chs = *(unsigned int *)&mbr[entryStart]; // CHS
        unsigned char tipo = mbr[entryStart + 4]; // Tipo
        unsigned int tam = *(unsigned int *)&mbr[entryStart + 12]; // Tamaño

        if (startSector != 0) {
            Particion *temp = realloc(particiones, (numParticiones + 1) * sizeof(Particion));
            if (temp == NULL) {
                fprintf(stderr, "Error al asignar memoria.\n");
                free(particiones);
                return 1;
            }
            particiones = temp;

            particiones[numParticiones].startSector = startSector;
            particiones[numParticiones].chs = chs;
            particiones[numParticiones].tipo = tipo;
            particiones[numParticiones].lba = startSector; // LBA es el mismo que startSector
            particiones[numParticiones].tam = tam;

            numParticiones++;
        }
    }

    int seleccion = 0;
    if (particiones != NULL) {
        printw("Seleccionar particion\n\n");
        seleccion = seleccionaParticion(numParticiones, particiones);
    } else {
        return -1;
    }

    free(particiones);
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

int leerDescriptorBloques(int fd,int inicio_descriptor_bloques,int imprimirInfo){
    
    // Lee el descriptor de bloques
    struct ext4_group_desc descriptor_bloques;
    if (pread(fd, &descriptor_bloques, sizeof(descriptor_bloques), inicio_descriptor_bloques) != sizeof(descriptor_bloques)) {
        perror("Error leyendo el superbloque");
        return -1;
    }
    if (imprimirInfo){
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
    }
    return descriptor_bloques.bg_inode_table_lo;
}

void abrirArchivoSeleccionado(int fd, struct ext4_dir_entry_2 *entradaSeleccionada, int inicioParticion,int inicio_superbloque) {
    // Calcular la posición del inodo en el disco
    unsigned int grupoInodes = entradaSeleccionada->inode / 2040;
    if (grupoInodes != 0){
        entradaSeleccionada->inode = entradaSeleccionada->inode % 2040;
    }
    unsigned int tamanoGDT = grupoInodes * 0x40;
    unsigned int descriptor = (inicio_superbloque + 0x400) + tamanoGDT;
    unsigned int inicio= leerDescriptorBloques(fd, descriptor,0);
    unsigned int inicioInodeArchivo = (inicio * 0x400) + inicioParticion + (0x100*(entradaSeleccionada->inode - 1));
    
    // Leer el inodo del archivo
    struct ext4_inode inode;
    if (pread(fd, &inode, sizeof(inode), inicioInodeArchivo) != sizeof(inode)) {
        perror("Error leyendo el inode del archivo");
        return;
    }

    // Crear un archivo temporal para escribir el contenido completo del archivo
    char nombreTemporal[] = "tempXXXXXX";
    int fd_temp = mkstemp(nombreTemporal);
    if (fd_temp == -1) {
        perror("Error creando archivo temporal");
        return;
    }

    // Asumiendo que el archivo es pequeño y cabe en los bloques directos
    for (int i = 12; i >=0; i--) {
        if (inode.i_block[i] != 0) {
            char bloque[1024]; // Asumiendo un tamaño de bloque de 1024 bytes
            int direccion_bloque = (inode.i_block[i] * 0x400) + inicioParticion;
            if (pread(fd, bloque, sizeof(bloque), direccion_bloque) != sizeof(bloque)) {
                perror("Error leyendo el bloque de datos del archivo");
                close(fd_temp);
                remove(nombreTemporal);
                return;
            }
            // Escribir el contenido del bloque al archivo temporal
            write(fd_temp, bloque, sizeof(bloque));
        }
    }
    close(fd_temp);

    // Llamar a hexvisor con el archivo temporal
    edita(nombreTemporal);

    // Eliminar el archivo temporal después de usarlo
    remove(nombreTemporal);
}

struct ext4_dir_entry_2 **leerBloque(int fd, int inicioBloque, int *numEntradas){
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

        if (entrada.rec_len == 0 || entrada.name_len == 0 || entrada.inode == 65280) {
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

int leerInodes(int fd, int inicioInode, int inicioParticion, int inicio_superbloque){
    clear(); // Limpiar la pantalla
    struct ext4_inode inode;
    if (pread(fd, &inode, sizeof(inode), inicioInode) != sizeof(inode)) {
        printw("Error leyendo el inode");
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
        mostrarBarraDeEstadoDir();
        refresh(); // Actualizar la pantalla
        int ch = getch(); // Obtener la tecla presionada
        if (ch == 3) { // esc
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
            if (entradas[seleccion]->file_type == EXT4_FT_DIR){ // Asumiendo que es un archivo regular
                continuar = 0;  // Cambiar la variable de control para salir del bucle
            }else{
                abrirArchivoSeleccionado(fd, entradas[seleccion], inicioParticion,inicio_superbloque);
            }
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

    int inicio_tabla_inodes = leerDescriptorBloques(fd,inicio_superbloque+0x400,1);

    long inicioInodes = (inicio_tabla_inodes * 0x400) + inicioParticion + 256;
    int numeroInode = leerInodes(fd,inicioInodes,inicioParticion, inicio_superbloque);

    while(numeroInode >0){
        unsigned int grupoInodes = numeroInode / 2040;
        if (grupoInodes != 0){
            numeroInode = numeroInode % 2040;
        }
        unsigned int tamanoGDT = grupoInodes * 0x40;
        unsigned int descriptor = (inicio_superbloque + 0x400) + tamanoGDT;
        unsigned int inicio= leerDescriptorBloques(fd, descriptor,0);
        inicio = (inicio * 0x400) + inicioParticion + (0x100*(numeroInode - 1));
        numeroInode = leerInodes(fd,inicio, inicioParticion,inicio_superbloque);
    }

    endwin();

    close(fd);

    return 0;
}
