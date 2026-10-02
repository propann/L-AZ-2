# Audit continu AZ-2 — état du dépôt au 19 septembre 2026

Branche `az2-screen-engines-sequencer`, base `813b05a`. Cette note complète l'audit de code du même jour, qui portait sur une révision antérieure. Les trois fichiers de firmware déjà modifiés dans l'arbre de travail ont été examinés sans effacer ces changements.

## Constats vérifiés dans le code

- Le chargement de projet transmet désormais `INST:...:255` : le défaut P1 d'instrument de pas non réinitialisé est corrigé dans le code. Cycle réel sauvegarde → modification → chargement encore à vérifier sur la machine.
- Les sauvegardes projet et patch passent désormais par un fichier temporaire et un backup. Cette passe vérifie l'échec du premier renommage et ouvre le backup si le fichier principal manque après une coupure.
- Poursuite de la phase 10 : les nouveaux fichiers portent `AZ2V2` et un CRC32 final. Le CRC est vérifié avant tout envoi au Teensy ; si le fichier principal échoue, le backup est essayé. Les anciens fichiers restent lisibles sans CRC. Le projet est refusé si les lignes obligatoires manquent, si des indices piste/pattern/pas sont hors plage ou dupliqués, ou si les valeurs principales dépassent les plages du protocole. Le fichier temporaire subit le même contrôle avant promotion.
- Le sampleur dispose maintenant de 16 pads, de WAV sur SD et d'un kit initial. La description « deux samples embarqués seulement » de l'audit précédent est périmée. `stopNow()` et `setSample()` sont maintenant encadrés par une courte suspension des mises à jour audio ; la lecture SD reste hors de cette section critique. Le gain de 0,7 sur chaque entrée ne garantit pas l'absence de saturation lorsque plusieurs pads jouent ensemble.
- La veille ne s'enclenche plus pendant un jeu GB chargé sur la page JEUX. Le comportement de retour à la veille après sortie du jeu reste à tester sur matériel.

## Risques ouverts, par priorité

1. **P1 — Intégrité des projets et patches.** Version, CRC32 et contrôle des lignes projet ajoutés ; restent à faire : valider exhaustivement les champs optionnels et les patches anciens, tester coupures et erreurs SD ainsi que le repli `.bak` sur matériel. Le CRC détecte une corruption des octets enregistrés ; le contrôle structurel détecte les lignes projet manquantes ou dupliquées.
2. **P1 — Cohérence du sampleur en temps réel.** Le changement d'état est maintenant synchronisé avec l'interruption audio. Tester sur matériel le remplacement d'un pad pendant sa lecture et mesurer les pics lorsque quatre à seize pads jouent simultanément.
3. **P1 — Sauvegarde GB.** La RAM cartouche dépend encore de `gbUnload()` ; ajouter une sauvegarde manuelle et périodique avec fichier temporaire et reprise.
4. **P2 — Séquenceur.** Mesurer la durée maximale de l'ISR et protéger les modifications de structures multi-champs effectuées par la boucle principale.
5. **P2 — Émulation.** Mesurer FPS logique et vidéo, audio et erreurs sur une matrice de ROMs. Les performances et la compatibilité ne sont pas encore démontrées.

## Validation

`tools/check_firmware_contract.py` passe ; les 14 tests natifs passent ; `pio run -e master_teensy -e screen_esp` compile les deux firmwares.

Essai matériel complémentaire : Teensy 4.1 identifié sur `/dev/ttyACM0`, écran ESP32-S3 via CH340 sur `/dev/ttyUSB0`. Les deux firmwares de cet arbre de travail ont été flashés avec succès ; vérification d'écriture ESP32 par hachage. Après redémarrage, le Teensy émet `STATUS:TEENSY_AUDIO:READY` et l'écran reçoit ce statut et émet `DISPLAY:ALIVE:TICK`. Aucun cycle réel save/load ni contrôle visuel/audio n'a encore été exécuté.

Après ajout de la section critique du sampleur, le firmware Teensy a été recompilé et reflashé avec succès. Le journal USB continue d'émettre `STATUS:TEENSY_AUDIO:READY` ; les compteurs `GB:AUDIO_RX` restent à zéro pendant l'observation au repos, sans erreur signalée. Le déclenchement et le remplacement d'un pad n'ont pas été exercés durant cette vérification.
