
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>    
#include <semaphore.h>  

#define DOSSIER_ENTREE "/home/adam/projet_signataire/dossier_entree"
#define SCRIPT_TRAITEMENT "/home/adam/projet_signataire/scripts/traiter.sh"

// On définit un pool de 4 "workers" 
#define NOMBRE_WORKERS 4

// La "liste de tâches" (le rail)
#define TAILLE_FILE 256


// La file d'attente des tâches
char *file_queue[TAILLE_FILE];
int file_count = 0;
int queue_head = 0;
int queue_tail = 0;

// Le Mutex qui va nous servir à bloquer des ressources 
pthread_mutex_t mutex_file;

// le sempahore qui va indiquer le nb de tache à faire 
sem_t sem_taches_disponibles;

// Reception des signaux du terminal ex : Controle + C ou systemd stop watcher 
volatile sig_atomic_t doit_terminer = 0;



/*
 * Bloc 2: Le Gestionnaire d'Arrêt (Signal Handler)
 */
void handle_signal(int sig) {
    doit_terminer = 1;
    // Prévient tous les thread qu'il y a une tache à faire (pour qu'ils prennent compte du signal )
    for (int i = 0; i < NOMBRE_WORKERS; i++) {
        sem_post(&sem_taches_disponibles);
    }
}


/*
 La fonction que nos threader vont utiliser
 */
void *worker_thread(void *arg) {
    long id = (long)arg;
    printf("Worker %ld: Démarré.\n", id);

    while (1) {
        
        // Le worker "dort" ici jusqu'à ce que sem_post() soit appelé
        sem_wait(&sem_taches_disponibles);

        // Si le thread est "reveillé" mais que c'est pr un signal de fin
        if (doit_terminer) {
            break; 
        }

        char *chemin_fichier;

        pthread_mutex_lock(&mutex_file); // reserve la liste de tâche pr empecher les conflits fichier pr lui
        
        chemin_fichier = file_queue[queue_head];
        queue_head = (queue_head + 1) % TAILLE_FILE;
        file_count--;

        pthread_mutex_unlock(&mutex_file); // Déverrouiller

        printf("Worker %ld: Traitement de %s\n", id, chemin_fichier);
       
        //création de l'enfant ici
        pid_t pid = fork();
       
        if (pid == 0) {
            // Processus Enfant
            execlp(SCRIPT_TRAITEMENT, SCRIPT_TRAITEMENT, chemin_fichier, NULL);
            perror("execlp"); // Si on arrive ici, c'est un échec car normalement code remplacer par traiter.sh
            exit(1);
        } else if (pid > 0) {
            // Processus Parent (le thread)
            waitpid(pid, NULL, 0); // On attend la fin du script
            printf("Worker %ld: Traitement de %s terminé.\n", id, chemin_fichier);
        } else {
            perror("fork");
        }
        
        // Le Producteur a fait malloc(), le Consommateur fait free()
        free(chemin_fichier);
    }

    printf("Worker %ld: Arrêt.\n", id);
    return NULL;
}


int main() {

    pthread_t workers[NOMBRE_WORKERS];
    int fd_inotify;
    struct sigaction sa;

    printf("Service Signataire : Démarrage...\n");

    // 1. Initialiser le Cadenas et le Compteur
    pthread_mutex_init(&mutex_file, NULL);
    sem_init(&sem_taches_disponibles, 0, 0); // 0 = partagé entre threads, 0 = valeur initiale

    // 2. Initialiser la Gestion des Signaux (Arrêt propre)
    sa.sa_handler = handle_signal;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);  // Pour Ctrl+C
    sigaction(SIGTERM, &sa, NULL); // Pour systemctl stop

    // 3. Lancer le Pool de thread 
    for (long i = 0; i < NOMBRE_WORKERS; i++) {
        pthread_create(&workers[i], NULL, worker_thread, (void*)i);
    }

    // 4. Initialiser inotify 
    fd_inotify = inotify_init();
    inotify_add_watch(fd_inotify, DOSSIER_ENTREE, IN_CREATE | IN_MOVED_TO);

    // --- Boucle Principale ---
    printf("Service en attente de fichiers...\n");
    char buffer[4096]; 

    while (!doit_terminer) {
        
         int length = read(fd_inotify, buffer, sizeof(buffer));
        if (length < 0) {
            if (doit_terminer) break; // Arrêt propre
            perror("read inotify");
            break; 
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            
            if (event->len > 0 && !doit_terminer) {
                
                // --- Ajout de Tâche (Protégé par le Cadenas) ---
                pthread_mutex_lock(&mutex_file);

                // On alloue de la mémoire pour le nom
                char *chemin_complet = malloc(512); 
                sprintf(chemin_complet, "%s/%s", DOSSIER_ENTREE, event->name);
                
                // On ajoute à la file
                file_queue[queue_tail] = chemin_complet;
                queue_tail = (queue_tail + 1) % TAILLE_FILE;
                file_count++;

                pthread_mutex_unlock(&mutex_file);

                sem_post(&sem_taches_disponibles); // Incrémente le compteur
                
                printf("Main: Fichier détecté: %s (Tâches en attente: %d)\n", event->name, file_count);
            }
            // On passe à l'événement suivant dans le buffer
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    // --- Arrêt Propre ---
    printf("Service Signataire: Arrêt en cours...\n");

    close(fd_inotify);

    // 8. Attendre que tous les thread finissent
    for (int i = 0; i < NOMBRE_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_destroy(&mutex_file);
    sem_destroy(&sem_taches_disponibles);

    printf("Service Signataire: Arrêté.\n");
    return 0;
}