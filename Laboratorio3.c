// Repositorio Github: git@github.com:Fermin555/Laboratorio-3.git
// Autores: Fermin Delgado, Kevin Ulloa

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>      // Para fork(), pipes, etc.
#include <sys/wait.h>    // Para wait() (esperar a los hijos)
#include <semaphore.h>   // Para manejar los semáforos
#include <sys/mman.h>    // Para mmap() (memoria compartida)
#include <fcntl.h>       // Para constantes de apertura de archivos
#include <string.h>

// 1. Defino QUÉ vamos a compartir entre los procesos
typedef struct {
    double saldo;
    sem_t semaforo;
} DatosCompartidos;

// 2. Prototipos de las funciones que van a ejecutar los hijos
void debito(char *archivo_montos, int p[], DatosCompartidos *memoria);
void credito(char *archivo_montos, int p[], DatosCompartidos *memoria);

int main() {
    
    printf("Programa iniciado...\n");

    return 0;
}