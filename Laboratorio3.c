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
    // Variables para los pipes (arreglos de 2 enteros) y los IDs de los procesos
    int pipe_credito[2];
    int pipe_debito[2];
    pid_t pid_credito, pid_debito;

    printf("Padre: Preparando el entorno...\n");

    // 1. Crear la Memoria Compartida
    // Pedimos al sistema un bloque del tamaño de nuestra estructura
    DatosCompartidos *memoria = mmap(NULL, sizeof(DatosCompartidos), 
                                     PROT_READ | PROT_WRITE, 
                                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    if (memoria == MAP_FAILED) {
        perror("Error al crear la memoria compartida");
        exit(1);
    }

    // 2. Inicializar los datos compartidos
    memoria->saldo = 0.0; // El saldo arranca en cero
    
    // Inicializamos el semáforo. 
    // Argumentos: puntero al semáforo, 1 (para compartir entre procesos), 1 (valor inicial: libre)
    if (sem_init(&memoria->semaforo, 1, 1) == -1) {
        perror("Error al inicializar el semáforo");
        exit(1);
    }

    // 3. Crear los Pipes (Walkie-talkies)
    if (pipe(pipe_credito) == -1 || pipe(pipe_debito) == -1) {
        perror("Error al crear los pipes");
        exit(1);
    }

    printf("Padre: Memoria, semáforo y pipes listos.\n");

    // 4. Creamos el primer hijo (Proceso Crédito)
    pid_credito = fork();

    if (pid_credito < 0) {
        perror("Error al crear el hijo credito");
        exit(1);
    } else if (pid_credito == 0) {
        // --- ESTE BLOQUE SOLO LO EJECUTA EL HIJO 1 ---
        // El hijo 1 solo usa su pipe para enviarle (escribir) al padre.
        close(pipe_credito[0]); // Por seguridad, cerramos la "oreja" (lectura) de su walkie-talkie
        close(pipe_debito[0]);  // Tampoco usa el pipe del otro hermano
        close(pipe_debito[1]);

        // Lo mandamos a trabajar con su función (la escribimos más adelante)
        credito("credito.txt", pipe_credito, memoria);
        
        // Cuando termina su trabajo, el hijo DEBE morir acá. No sigue de largo.
        exit(0); 
    }

    // 5. Creamos el segundo hijo (Proceso Débito)
    // Ojo: Solo el PADRE sobrevive para llegar a esta línea
    pid_debito = fork();

    if (pid_debito < 0) {
        perror("Error al crear el hijo debito");
        exit(1);
    } else if (pid_debito == 0) {
        // --- ESTE BLOQUE SOLO LO EJECUTA EL HIJO 2 ---
        close(pipe_debito[0]);  // Cerramos la "oreja" (lectura) de su walkie-talkie
        close(pipe_credito[0]); // No usa el pipe del hermano
        close(pipe_credito[1]);

        // Lo mandamos a trabajar
        debito("debito.txt", pipe_debito, memoria);
        
        exit(0); // El hijo 2 termina su vida acá.
    }

    // 6. --- ESTE BLOQUE SOLO LO EJECUTA EL PADRE ---
    // Como el padre solo va a ESCUCHAR (leer) lo que le mandan los hijos,
    // cerramos la "boca" (escritura) de ambos walkie-talkies.
    close(pipe_credito[1]);
    close(pipe_debito[1]);

    printf("Padre: Hijos creados correctamente. Esperando los reportes...\n");


    return 0;
}