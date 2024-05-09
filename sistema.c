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

# define INICIO_PARTICION 0x100000

// Función para leer un bloque de datos
int leer_bloque(int fd, int numero_bloque, size_t tamano_bloque, void *buffer) {
    off_t offset = (off_t)numero_bloque * tamano_bloque;
    ssize_t result = pread(fd, buffer, tamano_bloque, offset);
    return result == tamano_bloque ? 0 : -1;
}

void *leer_inode(int fd, int numero_inode, size_t tamano_bloque, size_t tamano_inode, off_t inicio_tabla_inodes) {
    // Asegúrate de que el offset esté alineado a una página
    off_t base_offset = (off_t)(numero_inode - 1) * tamano_inode + inicio_tabla_inodes;
    off_t aligned_offset = base_offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
    
    // Calcula el desplazamiento dentro de la página donde se encuentra el inode
    off_t intra_page_offset = base_offset - aligned_offset;

    // Mapea la página que contiene el inode
    void *mapped_page = mmap(NULL, tamano_inode, PROT_READ, MAP_PRIVATE, fd, aligned_offset);
    if (mapped_page == MAP_FAILED) {
        perror("Error mapeando el inode");
        return NULL;
    }

    // Devuelve un puntero al inode dentro de la página mapeada
    return (char*)mapped_page + intra_page_offset;
}


int main() {

    initscr();
	raw(); 
	keypad(stdscr, TRUE);	/* Para obtener F1,F2,.. */
	noecho();

    int fd = open("imagen.img", O_RDONLY);
    if (fd < 0) {
        perror("Error al abrir la imagen del sistema de archivos");
        endwin();
        return 1;
    }

    int inicio_superbloque = INICIO_PARTICION + 0x400;

    // Lee el superbloque
    struct ext4_super_block sb;
    if (pread(fd, &sb, sizeof(sb), inicio_superbloque) != sizeof(sb)) {
        perror("Error leyendo el superbloque");
        close(fd);
        endwin();
        return 1;
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
    refresh();
    getchar();
    clear();

    int inicio_descriptor_bloques = inicio_superbloque + 0x400;
    // Lee el descriptor de bloques
    struct ext4_group_desc descriptor_bloques;
    if (pread(fd, &descriptor_bloques, sizeof(descriptor_bloques), inicio_descriptor_bloques) != sizeof(descriptor_bloques)) {
        perror("Error leyendo el superbloque");
        close(fd);
        return 1;
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
    refresh();
    getchar();
    clear();



    // // Calcula el inicio de la tabla de inodes
    // off_t inicio_tabla_inodes = sb.s_first_data_block * tamano_bloque;
    // printw("Inicio de la tabla de inodes: %ld\n", (long)inicio_tabla_inodes);

    // // Lee el inode 2 (directorio raíz)
    // printw("Leyendo el inode número 2 (directorio raíz)...\n");
    // struct ext4_inode *inode_raiz = leer_inode(fd, 2, tamano_bloque, tamano_inode, inicio_tabla_inodes);
    // if (inode_raiz == NULL) {
    //     // Manejar error
    //     close(fd);
    //     return 1;
    // }

    // // Procesa el inode aquí
    // // Por ejemplo, puedes imprimir algunos de los atributos del inode:
    // printw("Inode 2: modo = %o, tamaño = %lu\n", inode_raiz->i_mode, tamano_inode);

    // // No olvides desmapear el inode después de usarlo
    // munmap(inode_raiz, tamano_inode);

    // // Cierra el archivo de imagen
    // close(fd);
    // printw("Archivo de imagen cerrado.\n");

    // return 0;
    endwin();
    return 0;
}
