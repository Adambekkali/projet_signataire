# Signataire Automatique (Projet Portfolio BC2)

`Signataire Automatique` est un service système événementiel pour Linux. Il surveille un dossier et applique un script de "signature" (traçabilité) à tout nouveau fichier, avant de le déplacer.

Ce projet a été développé pour démontrer la maîtrise des concepts de programmation système, de concurrence et de gestion des processus/threads sous Linux, dans le cadre de l'évaluation du Bloc de Compétence 2.

##  Compétences Démontrées

* **Appels Système (Bas Niveau) :** `inotify` pour une surveillance événementielle (0% CPU) au lieu du *polling*.
* **Gestion de Processus :** Utilisation du pattern `fork()` / `execvp()` / `waitpid()` pour exécuter des scripts externes (`traiter.sh`).
* **Multithreading :** Un *pool* de threads `pthread` (Consommateurs) pour traiter plusieurs fichiers en parallèle.
* **Synchronisation (Mutex) :** `pthread_mutex_t` pour protéger la file d'attente partagée (la *section critique*) contre les *race conditions*.
* **Synchronisation (Sémaphore) :** `sem_t` pour une gestion efficace de l'attente des threads *workers* (attente bloquante sans *polling*).
* **Gestion des Signaux :** Interception de `SIGINT` et `SIGTERM` (via `sigaction`) pour un arrêt propre et contrôlé.
* **Scripts Bash :** Le *worker* (`traiter.sh`) est un script shell gérant les I/O et les permissions (`chmod`, `mv`).

##  Architecture : Producteur-Consommateur

Le projet est bâti sur un modèle Producteur-Consommateur pour gérer efficacement un afflux massif de fichiers.



1.  **Le Producteur (Thread `main`)** : Utilise `inotify` pour détecter les fichiers. Il ajoute les tâches (noms de fichiers) à la file d'attente.
2.  **La File (Ressource Partagée)** : Une file circulaire (ring buffer) protégée par un **Mutex**.
3.  **Les Consommateurs (Pool de `pthread`)** : Un pool de 4 *workers*. Ils dorment sur un **Sémaphore**. Le `sem_post()` du producteur les réveille pour qu'ils prennent une tâche, appellent `fork()`, et lancent le script.

##  Compilation

Le projet est en C standard (C99) et Bash. Il n'a pas de dépendances externes, hormis `pthread`.

1.  **Compiler le programme principal :**
    ```bash
    gcc watcher.c -o watcher -lpthread
    ```

2.  **Rendre le script exécutable :**
    ```bash
    chmod +x scripts/traiter.sh
    ```

##  Utilisation (Test Manuel)

1.  Assurez-vous que les chemins (`DOSSIER_ENTREE`, `SCRIPT_TRAITEMENT`) dans `watcher.c` et `traiter.sh` sont corrects.

2.  Lancez le service dans un terminal :
    ```bash
    ./watcher
    ```
3.  Dans un *autre* terminal, déposez des fichiers pour tester :
    ```bash
    # Test simple
    echo "Test 1" > dossier_entree/test1.txt
    
    # Test de rafale
    touch dossier_entree/{a,b,c,d,e}.txt
    ```
5.  Observez le premier terminal pour voir les logs des *workers* et vérifiez le contenu du dossier `dossier_sortie`.

6. Vous pouvez également suivre les logs aves la commande 
    ```bash
 tail -f /tmp/signataire.log
    ```
