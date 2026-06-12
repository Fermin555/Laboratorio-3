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

// FUNCIÓN CRÉDITO (Suma al saldo)
void credito(char *archivo_montos, int p[], DatosCompartidos *memoria) {
    // 1. Abrimos el archivo en modo lectura ("r")
    FILE *archivo = fopen(archivo_montos, "r");
    if (archivo == NULL) {
        perror("Hijo Crédito: Error al abrir el archivo");
        exit(1);
    }

    double monto;
    
    // 2. Leemos el archivo línea por línea hasta que no haya más números
    while (fscanf(archivo, "%lf", &monto) == 1) {
        
        // --- SECCIÓN CRÍTICA (Uso del semáforo) ---
        // sem_wait es como pedir permiso. Si otro lo está usando, se queda esperando acá.
        sem_wait(&memoria->semaforo); 
        
        memoria->saldo += monto; // Sumamos a la pizarra compartida
        
        // sem_post es soltar el bastón para que otro pueda entrar
        sem_post(&memoria->semaforo);
        // ------------------------------------------

        // 3. Le mandamos el número al padre por el walkie-talkie (pipe de escritura)
        write(p[1], &monto, sizeof(double));
    }

    // 4. Limpieza del hijo
    fclose(archivo);
    close(p[1]); // Cerramos la boca del walkie-talkie. Esto le avisa al padre que terminamos (read devuelve 0).
}


// FUNCIÓN DÉBITO (Resta al saldo)
void debito(char *archivo_montos, int p[], DatosCompartidos *memoria) {
    // 1. Abrimos el archivo en modo lectura ("r")
    FILE *archivo = fopen(archivo_montos, "r");
    if (archivo == NULL) {
        perror("Hijo Débito: Error al abrir el archivo");
        exit(1);
    }

    double monto;
    
    // 2. Leemos el archivo línea por línea
    while (fscanf(archivo, "%lf", &monto) == 1) {
        
        // --- SECCIÓN CRÍTICA ---
        sem_wait(&memoria->semaforo); 
        
        memoria->saldo -= monto; // Restamos a la pizarra compartida
        
        sem_post(&memoria->semaforo);
        // -----------------------

        // 3. Le mandamos el número al padre
        write(p[1], &monto, sizeof(double));
    }

    // 4. Limpieza del hijo
    fclose(archivo);
    close(p[1]); // Le avisamos al padre que ya no vamos a mandar más nada.
}

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

    // Variables para el bucle de escucha
    double monto_leido;
    int activo_credito = 1; // 1 significa que sigue vivo, 0 que ya terminó
    int activo_debito = 1;

    // Mientras al menos un hijo siga activo
    while (activo_credito == 1 || activo_debito == 1) {
        
        // Escuchamos al hijo Crédito
        if (activo_credito == 1) {
            // La función read devuelve cuántos bytes pudo leer. 
            // Si devuelve > 0, es que llegó un número. Si devuelve 0, el hijo cerró el tubo.
            if (read(pipe_credito[0], &monto_leido, sizeof(double)) > 0) {
                printf("Padre: El hijo CRÉDITO sumó $%.2f\n", monto_leido);
            } else {
                activo_credito = 0; // El pipe se cerró, dejamos de escucharlo
            }
        }

        // Escuchamos al hijo Débito
        if (activo_debito == 1) {
            if (read(pipe_debito[0], &monto_leido, sizeof(double)) > 0) {
                printf("Padre: El hijo DÉBITO restó $%.2f\n", monto_leido);
            } else {
                activo_debito = 0; // El pipe se cerró, dejamos de escucharlo
            }
        }
    }

    // 7. Esperamos a que los hijos mueran formalmente (evita los procesos "zombie")
    wait(NULL);
    wait(NULL);

    // 8. Imprimimos el saldo que quedó en la pizarra compartida
    printf("\n========================================\n");
    printf("  SALDO FINAL: $%.2f\n", memoria->saldo);
    printf("========================================\n");

    // 9. Limpieza general 
    close(pipe_credito[0]);         // Cerramos los pipes de lectura que usaba el padre
    close(pipe_debito[0]);
    sem_destroy(&memoria->semaforo); // Destruimos el semáforo
    munmap(memoria, sizeof(DatosCompartidos)); // Devolvemos la memoria compartida al SO

    printf("Programa finalizado correctamente.\n");

    return 0;
}