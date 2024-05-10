#ifndef HEX_EDITOR_H
#define HEX_EDITOR_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <curses.h>
#include <sys/stat.h>
#include <sys/mman.h>

void edita(char *filename);
#endif

/* Variable global para mejor legibilidad */
int fd; // Archivo a leer
#define ANCHO_MAXIMO_LINEA 73 



char *hazLinea(char *base, int dir) {
	char linea[100]; // La linea es mas pequeña
	int o=0;
	// Muestra 16 caracteres por cada linea
	o += sprintf(linea,"%08x ",dir); // offset en hexadecimal
	for(int i=0; i < 4; i++) {
		unsigned char a,b,c,d;
		a = base[dir+4*i+0];
		b = base[dir+4*i+1];
		c = base[dir+4*i+2];
		d = base[dir+4*i+3];
		o += sprintf(&linea[o],"%02x %02x %02x %02x ", a, b, c, d);
	}
	for(int i=0; i < 16; i++) {
		if (isprint(base[dir+i])) {
			o += sprintf(&linea[o],"%c",base[dir+i]);
		}
		else {
			o += sprintf(&linea[o],".");
		}
	}
	sprintf(&linea[o],"\n");

	return(strdup(linea));
}

char *mapFile(char *filePath) {
    /* Abre archivo */
    fd = open(filePath, O_RDONLY);
    if (fd == -1) {
    	perror("Error abriendo el archivo");
	    return(NULL);
    }

    /* Mapea archivo */
    struct stat st;
    fstat(fd,&st);
    long fs = st.st_size;

    char *map = mmap(0, fs, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
    	close(fd);
	    perror("Error mapeando el archivo");
	    return(NULL);
    }

  return map;
}

int leeChar() {
  int chars[5];
  int ch,i=0;
  nodelay(stdscr, TRUE);
  while((ch = getch()) == ERR); /* Espera activa */
  ungetch(ch);
  while((ch = getch()) != ERR) {
    chars[i++]=ch;
  }
  /* convierte a numero con todo lo leido */
  int res=0;
  for(int j=0;j<i;j++) {
    res <<=8;
    res |= chars[j];
  }
  return res;
}

void mostrarBarraDeEstado() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x); // Obtiene las dimensiones de la ventana
    mvprintw(max_y - 1, 0, "CTRL+C: Salir | CTRL+G: Ir a | CTRL+B: Ir al principio | CRTL+F: Ir al final | Flechas: Navegar"); // Muestra los comandos
    clrtoeol(); // Limpia el resto de la línea
}



void edita(char *filename) {
	
    /* Limpia pantalla */
    clear();

    /* Lee archivo */
    char *map = mapFile(filename);
    if (map == NULL) {
      exit(EXIT_FAILURE);
    }

    
    for(int i= 0; i<25; i++) {
    	// Haz linea, base y offset
    	char *l = hazLinea(map,i*16);
	    move(i,0);
	    addstr(l);
    }
      initscr();
      curs_set(2); 
      mostrarBarraDeEstado();
    refresh();

    

    int ch = getch();
    int offset = 0;
    int x_offset = 0;
    struct stat st;
    fstat(fd, &st);
    int file_size = st.st_size; // Tamaño del archivo
	
    while ((ch = getch()) != 3) { // 26 es el código ASCII para CTRL+Z
        switch (ch) {
            case KEY_UP:
                if (offset > 0) offset -= 16; // Mueve hacia arriba en el archivo
                break;
            case KEY_DOWN:
                if (offset < file_size - (25 * 16)) offset += 16; 
                break;
            case 2: // Código ASCII para CTRL+B, equivalente a CTRL+< en algunos sistemas
                offset = 0; // Ir al inicio del archivo
                break;
            case 6: // Código ASCII para CTRL+F, equivalente a CTRL+> en algunos sistemas
                offset = file_size - (25 * 16); // Ir al final del archivo
                if (offset < 0) offset = 0; 
                break;
            case 7: // 7 es el código ASCII para CTRL+G
                echo(); // Habilita el eco para ver la entrada del usuario
                nodelay(stdscr, FALSE); // Desactiva el modo sin demora
                char str[10];
                mvprintw(26, 0, "Ir a la dirección (en hexadecimal): "); // Pide al usuario que ingrese la dirección
                getstr(str); // Obtiene la entrada del usuario
                offset = (int)strtol(str, NULL, 16); // Convierte la entrada a entero
                if (offset < 0) offset = 0;
                if (offset > file_size) offset = file_size - (25 * 16);
                move(26, 0); // Mueve el cursor a la línea del mensaje
                clrtoeol(); // Borra la línea donde estaba el mensaje
                noecho(); // Deshabilita el eco
                nodelay(stdscr, TRUE); // Reactiva el modo sin demora
                break;
            case KEY_LEFT:
                if (x_offset > 0) x_offset--; // Mueve hacia la izquierda en la línea
                break;
            case KEY_RIGHT:
                if (x_offset < ANCHO_MAXIMO_LINEA - 1) x_offset++; // Mueve hacia la derecha en la línea
                break;
        }
        usleep(100);
        // Actualiza la pantalla con el nuevo offset
        for(int i= 0; i<25; i++) {
            char *l = hazLinea(map, offset + i*16);
            move(i,0);
            addstr(l);
            free(l); // Libera la memoria
        }
        mostrarBarraDeEstado(); // Muestra la barra de estado
        // Mueve el cursor a la nueva posición dentro del área de contenido
        move(0, x_offset); // Mueve el cursor a la posición correcta
        refresh();
    }

    if (munmap(map, fd) == -1) {
      perror("Error al desmapear");
    }
    close(fd);
    

}