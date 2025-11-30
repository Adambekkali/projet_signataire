#!/bin/bash

# Le script est appelé avec le chemin du fichier en argument 1
CHEMIN_FICHIER=$1
# Récupère juste le nom du fichier
NOM_FICHIER=$(basename "$CHEMIN_FICHIER")

# Dossier de destination
DOSSIER_SORTIE="/dossier_sortie"
LOG_SERVICE="/tmp/signataire.log"

SIGNATURE="--- Traité par SIGNATAIRE BOT ESIEE IT le $(date) ---"

echo "" >> "$CHEMIN_FICHIER"
echo "$SIGNATURE" >> "$CHEMIN_FICHIER"

chmod 640 "$CHEMIN_FICHIER"

mv "$CHEMIN_FICHIER" "$DOSSIER_SORTIE/$NOM_FICHIER"

echo "[$(date)] Fichier '$NOM_FICHIER' signé et déplacé." >> "$LOG_SERVICE"
