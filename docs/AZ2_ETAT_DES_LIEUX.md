# AZ-2 - Etat des lieux

**[2026-09-18, nuit -- fiabilisation du fix filtre + test complet du
tracker/page PATCH par simulation, sans les mains sur le vrai clavier]**

Suite de l'entree du dessous (fix filtre 18kHz -> 15kHz). Deux
choses faites a la demande "on fiabilise et il faut que je teste ca du
tracker tu peut tester toi" :

- **Fiabilisation** : reteste le nouveau plafond (15000Hz) a
  RESONANCE MAXIMALE (127/127, pas juste 0.7 minimal comme lors de la
  decouverte) -- toujours propre, confirme utilisateur ("ca sonne
  propre"). Le nouveau plafond tient meme dans le pire cas (resonance
  au max), pas seulement au minimum ou le bug avait ete trouve.
- **Outil de test ajoute** (`src_teensy/az2_audio/main.cpp`) :
  `SIMNAV:<direction>:<0|1>` et `SIMBTN:<lettre>:<0|1>` sur le port USB
  du Teensy -- injectent un evenement croix/bouton EXACTEMENT comme un
  vrai appui physique (meme `az2::printNav()`/`printBtn()` vers
  Serial1, seul chemin par lequel l'ESP32 recoit ces evenements, la
  croix/les boutons etant cables directement sur le Teensy). Permet de
  piloter le tracker par script depuis l'ordinateur, sans avoir acces
  au clavier physique -- garde en permanence dans le firmware (outil
  de test utile, cout nul).
- **Test complet execute et REUSSI sur le vrai materiel** (verifie en
  lisant les commandes que l'ESP32 envoie en retour au Teensy sur son
  propre port de debug -- `sendToTeensy()` les imprime deja toutes,
  aucun ajout de code cote ESP32 necessaire) :
  - Menu -> categorie MUSIQUE -> SEQUENCEUR (2x BTN:A) : OK.
  - Page SEQUENCEUR, les 6 colonnes testees une par une (croix DROITE
    pour changer de colonne, A maintenu + HAUT pour editer) :
    - **NOTE** (colonne 0) : pas OFF au depart -> a bien envoye
      `STEP:0:0:1` PUIS `NOTE:0:0:61` -- confirme que le fix
      "auto-activation du pas en editant la note" (2026-09-18, plus
      haut dans ce journal) est toujours vivant et fonctionne.
    - **INST** -> `INST:0:0:1`. **FX** -> `SFX:0:0:1:0`. **VAL** ->
      `SFX:0:0:0:1`. **PROB** -> `PROB:0:0:100`. **COND** ->
      `COND:0:0:33` (= 1:2 dans le cycle, encodage nibble correct).
      Les 6 colonnes produisent la commande attendue, aucun crash, aucun
      blocage.
  - Retour Menu (BTN:B) -> categorie MUSIQUE -> DOWN x2 (MOTEURS puis
    PATCH) -> BTN:A : entree sur la page PATCH confirme par l'envoi
    automatique de `SCOPE:0` (voir `goTo()`).
  - Page PATCH, ligne 0 (filtre), A+HAUT -> `FILT:0:127:0` envoye : OK.
  - Ligne 7 (SLOT), A+DROITE -> `PATCH_SAVED:/patches/0.txt` (sauvegarde
    reelle sur SD). A+GAUCHE -> **recharge le meme fichier et renvoie
    exactement les memes parametres** (`ENGINE:0:4`, `PATCH:0:0`,
    `FILT:0:127:0`, `ENV:0:3:20:89:38`, `DXP:0:0:4`, `DXP:0:1:0`) --
    aller-retour sauvegarde/chargement de patch confirme integre et
    fonctionnel.
- **Limite honnete de ce test** : verifie la LOGIQUE (les bonnes
  commandes partent au bon moment, dans le bon format) via le journal
  serie, PAS le RENDU VISUEL a l'ecran (surlignage, texte affiche) --
  ca reste a l'utilisateur de confirmer que ce qui s'affiche correspond
  bien a ce que la logique fait. Etat remis a plat apres le test
  (demute tout, plus de solo, pas de test desactive).

**[2026-09-18, nuit -- RESOLU : le "bruit blanc" des moteurs (DEXED,
EPIANO, BRAIDS, KARPLUS, puis ANALOG aussi) n'etait pas un bug moteur --
c'etait le filtre partage, reglee par defaut dans sa zone d'instabilite]**

Suite de l'investigation des entrees precedentes ("cause toujours pas
trouvee"). Cause reelle trouvee et confirmee sur le vrai materiel :

- **Le defaut au boot ET le plafond de la commande FILT: (127/127)
  etaient a la MEME valeur : 18000Hz.** Donc TOUS les tests "moteur X
  sonne mal" depuis le debut de cette investigation (hier soir et ce
  soir) ont ete faits filtre grand ouvert a 18kHz, sans que personne
  (utilisateur ni moi) ne le sache -- le filtre n'a jamais ete suspecte
  parce que "grand ouvert" (127/127, le reglage par defaut affiche comme
  "neutre") semblait la position la moins susceptible de deformer le
  signal.
- `AudioFilterStateVariable` (topologie Chamberlin SVF, voir
  `filter_variable.h` dans la lib Audio Teensy) autorise en theorie
  jusqu'a `AUDIO_SAMPLE_RATE_EXACT/2.5` (~17647Hz, son propre clamp
  interne) mais devient en pratique instable/auto-oscillant pres de
  cette limite, MEME a resonance minimale (0.7, la plus "plate"
  possible ici) -- un defaut connu de cette topologie de filtre pres de
  Nyquist, jamais documente dans ce projet avant ce soir.
- **Teste en direct sur le vrai materiel, piste 0 isolee (MUTE sur les 7
  autres pistes + SOLO:0:1), meme note (60), memes 5 moteurs un par
  un** : a 18000Hz (defaut), bruit blanc reproductible identiquement
  sur DEXED/EPIANO/BRAIDS/KARPLUS **ET ANALOG** (le moteur jusque-la
  "epargne" -- premiere fois qu'il est retest apres le fix enveloppe
  partagee d'hier soir, et il echoue aussi cette fois, ce qui a
  relance l'enquete). A 8000Hz, 12000Hz puis 15000Hz : **note propre
  sur les 5 moteurs**, confirme utilisateur ("la ca sonne", "pareil
  une note qui monte en volume", "la ca sonne aussi de plus en plus
  fort"). A 18000Hz encore une fois (retour au defaut) : bruit blanc de
  nouveau. Correlation parfaite cutoff/bruit, aucune dependance au
  moteur -- preuve directe que la CAUSE COMMUNE speculee dans l'entree
  precedente ("chaine de signal... pas un bug isole par bibliotheque")
  etait juste, et que c'etait le filtre partage.
- **Corrige** (`src_teensy/az2_audio/main.cpp`) : plafond de
  `handleFiltCommand()` et defaut au boot passes de 18000Hz a 15000Hz
  (dernier point teste propre sur le vrai materiel -- 18000Hz ne
  l'etait pas). Flashe et reteste sur le vrai materiel SANS aucune
  commande FILT: (reglage par defaut du boot) : DEXED puis EPIANO/
  BRAIDS/KARPLUS/ANALOG tous confirmes propres par l'utilisateur.
- **Le fix "enveloppe partagee" d'hier soir (trackAnalogEnv[] commun
  aux 5 moteurs) reste dans le code** -- n'a jamais ete la cause, mais
  c'est une amelioration reelle et sans effet de bord (gating ADSR
  propre au lieu d'un simple mute par moteur), gardee.
- **Le bug de transmission SCOPE (entree du dessous) est une decouverte
  separee et reste corrige independamment** -- utile pour la fiabilite
  generale du lien, pas pour ce bug-ci (qui n'a jamais eu besoin du
  scope pour etre resolu, juste d'un test methodique cutoff par
  cutoff).
- **Lecon a retenir** : quand plusieurs bibliotheques totalement
  independantes (dont une classe officielle Teensy Audio Library)
  produisent EXACTEMENT le meme symptome, chercher dans ce qu'elles ont
  en commun (ici : le filtre partage en aval, identique pour toutes les
  pistes) avant de re-suspecter chaque bibliotheque individuellement.

**[2026-09-18, nuit -- bug de transmission SCOPE trouve et corrige,
flashe et lien confirme stable sur le vrai materiel (voir entree
au-dessus pour la suite -- ce correctif n'a PAS resolu le bruit moteur,
c'etait une piste independante)]**

Suite de l'entree precedente ("prochaine piste serieuse : reparer
d'abord le bug SCOPE"). Analyse complete du chemin
`updateScope()` (Teensy) -> `readTeensyStatus()`/`handleScopePacket()`
(ESP32), et de son jumeau symmetrique cote son GB
(`AudioRxState`/`readStream()` sur le Teensy, ESP32 -> Teensy) :

- **Cause racine identifiee** : (1) aucune verification de longueur ni
  checksum dans le parseur binaire -- un SEUL octet perdu decale la
  lecture "longueur" sur un octet de charge utile quelconque, qui peut a
  son tour valoir par hasard l'octet magique et relancer un faux paquet
  -- desynchronisation qui s'auto-entretient indefiniment une fois
  declenchee (explique le "bruit" aleatoire identique du dump SCOPE,
  qu'il vienne d'ANALOG propre ou d'un moteur casse : ce sont des
  octets de charge utile arbitraires mal alignes, pas une propriete du
  signal audio) ; (2) `handleScopePacket()` appelait `drawPatchScope()`
  (dessin SPI, plusieurs ms) DEPUIS la boucle meme qui lit `Serial1`
  octet par octet, bloquant la lecture assez longtemps pour perdre des
  octets ; (3) tampon RX materiel laisse a sa taille par defaut (256 o
  cote ESP32 ; petit aussi cote Teensy) alors que le lien tourne
  maintenant a 921600 bauds -- se remplit en 2-3 ms, moins que certains
  blocages du `loop()` (dessin ecran, frame GB), tout depassement perd
  des octets EN SILENCE (aucune notification d'overflow lue par le
  code).
- **Corrige (compile sur les 3 environnements + 9 tests natifs OK,
  PAS ENCORE flashe/verifie sur le vrai materiel a l'heure ou ceci est
  ecrit)** :
  - `Serial1.setRxBufferSize(2048)` cote ESP32 (avant `begin()`) et
    `Serial1.addMemoryForRead(buf, 2048)` cote Teensy (API native du
    core Teensy 4, avant `begin()`) -- marge large a 921600 bauds.
  - Dessin du tracer (`drawPatchScope()`) sorti de la boucle de lecture
    Serial1 -- `handleScopePacket()` ne fait plus que copier les
    echantillons et poser `scopeNeedsRedraw`, le dessin reel se fait une
    fois par tour de `loop()`, apres `readTeensyStatus()`.
  - Verification de longueur stricte des deux cotes : le Teensy
    n'envoie jamais qu'une longueur SCOPE fixe (32,
    `kScopeSamplesPerPacket`) et l'ESP32 n'envoie jamais qu'une longueur
    audio GB fixe (`kGbAudioSamplesPerPacket`, nouvelle constante
    partagee dans `AZ2_Protocol.h`, meme formule que `AUDIO_SAMPLES`
    dans `minigb_apu.h`) -- toute longueur recue differente = paquet
    rejete et resynchronisation immediate au lieu d'avaler des octets de
    charge utile bidon.
  - Delai d'abandon (20 ms, tres large face a un paquet complet en <1 ms
    a 921600 bauds) : un paquet reste "ouvert" trop longtemps (lien
    bloque/coupe au milieu) force un retour en mode texte au lieu de
    rester coince a attendre un octet qui n'arrivera jamais.
- **A faire ensuite** : flasher les deux cartes, reverifier que le
  dump SCOPE donne enfin une vraie forme d'onde coherente pour ANALOG
  (propre) ET pour un moteur casse (KARPLUS/BRAIDS/EPIANO), PUIS s'en
  servir comme vrai outil de diagnostic pour la cause du bruit moteur
  lui-meme (toujours inconnue, voir entree precedente). Profiter aussi
  de l'occasion pour re-verifier que le son GB (meme classe de
  correctif applique cote Teensy) reste stable au niveau audio/visuel a
  921600 bauds.

**[2026-09-18, soir -- bruit blanc elargi a 4 moteurs sur 5, cause
toujours pas trouvee malgre une investigation poussee]**

Retour utilisateur sur le vrai materiel : en testant EPIANO/BRAIDS/
KARPLUS un par un (meme protocole que le test DEXED d'hier), les 3
produisent **le meme bruit blanc, decrit comme identique** par
l'utilisateur. **Seul ANALOG reste confirme propre.** Ca elargit
fortement le probleme -- ce n'est PAS specifique a DEXED (ni a une
bibliotheque vendored en particulier, puisque KARPLUS est un objet
Teensy Audio Library OFFICIEL, pas du code tiers).

Tests faits pour isoler la cause :

- **MUTE:0:1 pendant que le bruit joue -> silence.** Confirme que le
  bruit vient bien de la piste 0 (pas d'une autre piste mal coupee).
- **FILT:0:0:0 (filtre ferme a 20Hz) -> silence total.** FILT:0:64:0
  (coupure intermediaire ~600Hz) -> **une vraie note courte redevient
  audible, melangee a des "petits clacs"**. Ca prouve que le moteur
  produit un signal musical REEL, pas juste du bruit -- quelque chose
  d'autre (haute frequence, large bande) le noie/domine a coupure
  grande ouverte (reglage par defaut).
- **Hypothese testee : enveloppe partagee.** `trackAnalogEnv[]`
  (jusqu'ici reservee a ANALOG) est devenue le point de passage
  OBLIGE des 5 moteurs avant le filtre (`patchTrackIn[]` alimente
  maintenant toujours l'enveloppe, pas le filtre directement ;
  `trackNoteOn()`/`trackNoteOff()` declenchent cette enveloppe pour
  tous). Raisonnement : les 4 moteurs casses alimentaient le filtre
  resonant DIRECTEMENT (sans jamais garantir un zero strict entre les
  notes), contrairement a ANALOG. **Teste sur le vrai materiel : AUCUN
  changement audible.** Le fix est garde (enveloppe ADSR partagee =
  amelioration raisonnable en soi, memes attaques/coupures plus
  propres a terme), mais ce n'etait PAS la cause du bruit.
- **Oscilloscope (SCOPE:) -- FAUSSE PISTE, retiree.** Dump brut des
  echantillons ajoute temporairement cote ESP32 (lecture directe sur
  Serial1, pas commis) : KARPLUS montre des valeurs qui sautent entre
  0 et 255 sans forme coherente -- semble confirmer du "vrai bruit".
  **Mais ANALOG (confirme propre a l'oreille) montre EXACTEMENT le
  meme genre de valeurs chaotiques** avec le meme outil. Conclusion :
  **le chemin de transmission SCOPE lui-meme a un bug** (corruption
  entre le Teensy et l'ESP32, ou dans la reception cote ESP32) --
  invalide toute conclusion tiree de ce dump. Code de dump retire
  (jamais commis dans l'historique). A investiguer separement, PAS un
  indice fiable pour le bruit des moteurs.

**Conclusion honnete** : la cause reste inconnue ce soir malgre
plusieurs pistes testees en direct sur le vrai materiel. Le pattern
(4 moteurs distincts, dont un objet Teensy Audio Library officiel,
touches identiquement ; seul ANALOG epargne) pointe vers quelque chose
de commun a la CHAINE DE SIGNAL ou a l'ALLOCATION MEMOIRE partagee
(voir aussi le tas critique -- 1.6 Ko libres -- note le 2026-09-18
matin), pas vers un bug isole dans une bibliotheque. Prochaine piste
serieuse : reparer d'abord le bug SCOPE (transmission des paquets
Teensy->ESP32), pour avoir un vrai outil de mesure de forme d'onde
avant de continuer a deviner.

**[2026-09-18, GB : rendu video corrige, UART monte a 921600, confirme
en reel]**

- **Regression video annulee** : le rendu "1 bloc PSRAM entier envoye a
  la fin de l'image" fusionne plus tot le meme jour (autre session IA,
  jamais teste sur le vrai materiel avant ce merge) causait un
  scintillement continu de toute l'image ("l'ecran scintille"). Revenu
  au rendu par bandes du 2026-09-15 (1 `draw16bitRGBBitmap()` par ligne
  source, jamais eu ce probleme). **Confirme sur le vrai materiel :
  "c'est beaucoup mieux"**. Walnut-CGB documente lui-meme une limite
  connue a ce style de rendu ligne par ligne (animations qui ne
  s'affichent pas toujours bien, ex. Prehistorik Man cite dans son
  README) -- accepte comme compromis du coeur d'emulation, pas un bug
  AZ-2 a chasser davantage pour l'instant.
- **UART ecran<->Teensy monte a 921600 bauds** (etait 230400) --
  prerequis du "Palier A" de AZ2_EMULATION_JEUX.md pour ameliorer la
  qualite audio du jeu sans saturer le lien. **Teste et confirme sur le
  vrai materiel** : round-trip complet (STEP:/NOTE:/BPM: envoyes cote
  Teensy, recus intacts cote ESP32), croix/boutons/ecran normaux,
  aucune corruption observee. Utilisateur : "ça tourne, c'est pas
  magique mais ça tourne" -- le debit est pret, **la qualite audio du
  jeu elle-meme n'a PAS encore ete touchee** (toujours 8kHz mono 8
  bits, "telephone" -- voir plan plus bas).
- **Recherche faite sur des emulateurs alternatifs** (CrankBoy, gnuboy)
  suite a la frustration utilisateur -- **conclusion : on garde
  Walnut-CGB**, c'est deja le choix le plus adapte a nos besoins exacts
  (support GBC complet + performance). CrankBoy n'a qu'un support GBC
  tres limite (pense pour la Playdate, monochrome) ; les projets a base
  de gnuboy (nesboy-esp32, GBCCAT) visent des ecrans SPI (ST7789/
  ILI9341), pas notre ecran RGB parallele -- meme probleme d'integration
  qui avait deja ecarte Retro-Go. Piste de qualite audio trouvee dans
  la doc Walnut-CGB elle-meme : elle recommande **Blargg's Gb_Snd_Emu**
  (LGPL, compatible) a la place de minigb_apu "si une emulation APU
  precise est necessaire" -- voir la feuille de route ci-dessous.

## Feuille de route GB/GBC (demandee par l'utilisateur, 2026-09-18)

Ordre propose, du plus pret au plus gros chantier :

1. **[FAIT]** Corriger le scintillement video (rendu par bandes).
2. **[FAIT]** Monter l'UART a 921600 bauds, verifier la fiabilite.
3. **Cible qualite audio concrete** : passer `AUDIO_SAMPLE_RATE` de
   8000 a 22050 Hz (ou tenter stereo 8 bits a debit egal -- voir Palier
   A) -- a 921600 bauds (~92 Ko/s bruts), un flux mono 8 bits a 22050 Hz
   (~22 Ko/s) tient large, meme avec le reste du trafic de controle.
   Changement de constante + verification du budget UART reel sur
   materiel (paquets perdus/en retard pendant le jeu).
4. **Integrer Gb_Snd_Emu (Blargg)** a la place de minigb_apu pour une
   emulation APU plus fidele -- chantier plus gros (vendoring, licence
   LGPL a documenter dans AZ2_LICENCES.md, adapter l'interface
   read/write registres au lieu de minigb_apu_audio_read/write). A
   faire une fois l'etape 3 validee (pas la peine d'ameliorer la
   precision APU si le flux qui la transporte est encore limite a
   8kHz).
5. **Protocole audio V2** (longueur 16 bits, numero de sequence,
   compteur de pertes -- deja recommande dans l'audit de progression
   du 2026-09-18) -- utile une fois 3-4 en place, pour diagnostiquer
   les eventuelles pertes a la nouvelle qualite plutot que deviner.
6. Video/frameskip reglable, palettes DMG, menu JEUX complet (voir
   AZ2_EMULATION_JEUX.md, sections "Video a ameliorer"/"Nouveau menu
   JEUX") -- apres que le son et l'image de base soient satisfaisants.

**[2026-09-18, retour utilisateur -- 26 commits d'une autre session IA
fusionnes, hypothese MSFA testee et ECARTEE]** Pendant l'absence de
l'utilisateur (parti au travail), une AUTRE session IA a travaille en
parallele directement sur `main` (26 commits) : audit de progression
complet, fix reproductibilite (lib MIDI morte retiree), perf/cadence
GB (rendu en un bloc PSRAM + pacing microseconde), CI (`ci.yml`,
reutilise `pio test -e native`), et une hypothese concrete sur DEXED --
le constructeur de `Dexed` choisit le coeur FM **MKI** par defaut,
jamais change, alors que **MSFA** est le coeur de reference historique
-- teste comme piste A/B. Fusionne sans aucun conflit (fast-forward,
convergence independante confirmee sur le retrait de DEXED des moteurs
par defaut -- memes valeurs des 2 cotes).

**Teste sur le vrai materiel au retour de l'utilisateur : l'hypothese
MSFA NE REGLE PAS le bruit** -- toujours "souffle, bruit blanc" avec
DEXED force sur MKI->MSFA, piste 0 isolee/solo, note tenue. Piste
ECARTEE. DEXED reste hors des moteurs par defaut (aucun changement de
ce cote), toujours selectionnable a la main pour continuer a le
diagnostiquer plus tard. Egalement fusionne : une exploration
d'architecture "AZ-3" (cartouches moteur ESP32/ESP8266 "AZ-VA1"/
"AZ-CHIP", rack "AZ-BUS") auto-retractee par cette meme session en fin
de parcours ("le boitier AZ-2 est plein, on reste sur le materiel
existant") -- gardee comme reference future, hors scope AZ-2 actuel
(confirme par l'utilisateur : "pas urgent").

**[2026-09-18, apres-midi -- travail en autonomie, ESP32 debranche]**
Utilisateur parti au travail ("continue par la suite"). Teensy encore
branche, ESP32 non -- travail limite au code compile-verifie (pas
flashe/teste sur l'ESP32, a confirmer au prochain reveil du materiel) :

- **Page PATCH, ligne SLOT (7e ligne) ajoutee au meme systeme de
  croix** que le reste de la page (voir le fix precedent) -- la ligne
  SLOT/SAVE/LOAD existait deja a l'ecran (tactile uniquement), pas
  besoin de redesign pour l'ajouter (contrairement a moteur/patch
  integre, qui eux ne rentrent pas). A maintenu + HAUT/BAS cycle le
  numero de slot ; A maintenu + GAUCHE/DROITE declenche SAVE/LOAD
  (nouvelle combinaison, ne touche pas GAUCHE/DROITE seuls qui restent
  le changement de piste). Compile verifie (3 environnements), **pas
  flashe/teste** (ESP32 debranche).
- **Fausse alerte SCOPE: corrigee** (voir plus bas dans le journal) --
  erreur de methode de test de ma part, pas un bug firmware.

**[2026-09-18 matin -- ESP32 flashe, les 2 cartes tournent ensemble]**

**[2026-09-18, demande UI/UX -- gros chantier note, pas fait cette
session]** Retour utilisateur en testant le tracker sur le vrai
materiel : le panneau lateral "patch actif" (`drawTrkSidePanel()`,
cote droit de la page SEQUENCEUR) est juge inutilisable ("on peut rien
faire, y'a rien qui marche"). Demande precise pour le refaire :

1. **Navigation par bouton, surbrillance a presser pour changer la
   valeur** -- meme logique que les colonnes NOTE/INST/FX/VAL/PROB/COND
   du tracker (voir seqDetailCol), pas un panneau juste affiche.
2. **Le panneau devient 2 raccourcis empiles** au lieu d'un bloc
   d'infos statique : en haut un raccourci "PATCH" qui ouvre une
   fenetre dediee de reglage de patch (moteur, forme d'onde/algo
   selon le moteur) ; en dessous un raccourci "EFFETS" qui ouvre une
   fenetre de reglage d'effets **PAR PISTE**, activables/reglables --
   n'existe pas encore cote Teensy (FX:reverb/FX:delay actuels sont
   sur le bus MAITRE global via potards 2/3, pas par piste -- vraie
   extension d'architecture a concevoir, pas juste un ecran).
3. **Page AUDIO (clavier tactile pads)** : passer en plein ecran
   (actuellement partage l'ecran avec d'autres elements), ajouter une
   rangee de boutons pour changer la gamme a la volee, et un
   **arpegiateur complet** (pas juste le triangle ARP par pas du
   tracker actuel -- un vrai mode de jeu arpegiateur en temps reel sur
   le clavier live). Objectif explicite : "que notre petit clavier
   4x4 soit top".

Pas commence cette session (gros chantier UI + nouvelle architecture
d'effets par piste + fonctionnalite musicale complete a concevoir
proprement, pas a bacler en direct pendant une session de debug
materiel). A traiter dans une session dediee.

**Precision donnee plus tard dans la meme conversation** : le panneau
affiche aujourd'hui le moteur + la pattern (deja visibles ailleurs a
l'ecran -- redondant, "on l'a deja a cote, pas besoin de rappel").
Remplacer ce contenu par des BOUTONS qui appellent chacun un ecran de
reglage dedie : choix du moteur, choix/reglage de l'effet assigne a la
piste, reglages moteur (filtre/ADSR/algo selon le moteur), clavier
tactile, arpegiateur -- "que tous les outils soient disponibles,
appelables par des boutons dans le cadre". Confirme la meme direction
que les points 1-3 ci-dessus, precise le contenu exact des boutons a
prevoir.

**Spec complete donnee ensuite (meme conversation, apres test du fix
croix de la page PATCH -- voir plus bas)** :

- **Page MOTEURS** : doit devenir une vraie LISTE de tous les moteurs
  disponibles (DEXED/EPIANO/BRAIDS/KARPLUS/ANALOG), pour attribuer l'un
  d'eux a la piste choisie -- pas juste cycler un a la fois comme
  aujourd'hui.
- **Page PATCH** : gerer les patchs de TOUS les moteurs -- selectionner
  (le patch integre du moteur, ex. "FM-Rhodes"), regler (deja fait :
  filtre/ADSR/DXP via la croix, voir fix plus bas), **sauvegarder**
  (existe deja cote tactile -- `savePatchSlot()`/`loadPatchSlot()`/
  `patchSlot`, voir le code -- juste pas encore accessible au bouton/
  croix). Le choix du MOTEUR lui-meme et du PATCH INTEGRE ne sont PAS
  pilotables au bouton sur cette page actuellement (seuls filtre/ADSR/
  volume le sont depuis le fix ci-dessous) -- **pas assez de place
  verticale dans la mise en page actuelle** (6 lignes + volume
  remplissent deja l'ecran) pour rajouter ces 2 controles sans repenser
  la mise en page -- **vrai redesign a faire**, pas un ajustement.
- **Nouvel outil EFFET** (n'existe pas) : choisir la piste, activer/
  regler un effet, sauvegarder -- effets PAR PISTE, extension
  d'architecture cote Teensy (voir point 2 plus haut, FX:reverb/delay
  actuels sont sur le bus maitre global uniquement).
- **Les 2 fenetres PATCH et EFFET appelables depuis le panneau lateral
  du tracker** (memes boutons que decrits plus haut).

Toujours pas commence (meme raison : gros chantier de mise en page +
nouvelle architecture d'effets, a faire dans une session dediee, pas en
direct).

**[2026-09-18, suite du diagnostic DEXED]** Ajoute `checkHeap()` /
commande `HEAP?` (mallinfo(), voir le commentaire dans le code) pour
verifier la piste "allocation echouee silencieusement" (`new` sans
verification dans `Dexed::Dexed()`, voir plus bas). Resultat mesure sur
le vrai materiel, au repos, apres boot : `used=34800:
free_in_arena=1616:arena=36416` -- seulement 1.6 Ko de marge dans le
tas actuellement obtenu du systeme. Serre, un vrai point a corriger
(augmenter la marge, ou reduire le nombre d'instances Dexed
simultanees), **mais probablement pas la cause principale du bruit** :
une allocation ratee produirait plutot un comportement casse/incoherent
tout de suite, pas "silence tant qu'on ne joue pas, bruit uniquement
en jouant une note, jusqu'a redemarrage complet" -- ce dernier pattern
ressemble plus a une corruption d'etat numerique DANS le calcul FM une
fois une voix declenchee (pas a une allocation qui echoue).

**[CORRIGE] Fausse alerte sur SCOPE:, erreur de methode de test, pas un
bug** : je capturais `/dev/ttyACM0` (port USB du Teensy, `Serial`) en
cherchant les paquets binaires du scope -- mais `updateScope()` les
ecrit sur `Serial1` (`Serial1.write(...)`, voir le code), la liaison
UART DEDIEE vers l'ESP32, jamais visible sur le port USB. Rien ne
prouve que le scope soit casse ; je n'ai simplement jamais pu
l'observer avec les outils que j'avais sous la main (pas d'acces a la
liaison UART Teensy<->ESP32 directement, seulement aux 2 ports USB).
**A verifier plutot en regardant l'ecran** : la page PATCH doit
afficher une forme d'onde en jouant une note sur la piste choisie --
verification visuelle, pas cote serie. Si le scope marche vraiment (a
confirmer), il reste l'outil naturel pour visualiser precisement ce
que produit DEXED plutot que d'en deviner la forme.

**Conclusion honnete** : la vraie cause du bruit DEXED (probablement
dans le coeur FM de `src_teensy/microdexed-touch/third-party/
Synth_Dexed/src/` -- fm_core.cpp/fm_op_kernel.cpp/dx7note.cpp) demande
de VOIR la forme d'onde reelle pour etre diagnostiquee sans deviner --
la page PATCH (SCOPE:) est l'outil pour ca, a verifier a l'ecran au
prochain reveil du materiel plutot qu'a l'aveugle. Pas fait cette
session -- DEXED n'est plus le moteur par defaut d'aucune piste (voir
plus bas), evite tant que non repare.

**[2026-09-18, session de flash -- BUG REEL confirme sur DEXED, pas
materiel]** Long diagnostic en direct avec l'utilisateur sur un
"souffle" audio signale des le premier flash. Piste materielle (SCK/GND
du PCM5102A) explorée et ecartee : ressoudee par l'utilisateur, souffle
inchange. Diagnostic definitif par elimination binaire (MUTE: piste par
piste, 8 tests) :

- **Toutes les pistes coupees = silence total.** Chaque piste
  DEMUTEE INDIVIDUELLEMENT (0 a 7, une par une, testee sur le vrai
  materiel) = silence, aucune ne produit de bruit seule au repos.
- **Le GB (ESP32, meme DAC final) est confirme propre** par
  l'utilisateur -- ecarte un probleme materiel/DAC/SCK generique,
  toute la chaine de sortie fonctionne.
- **DEXED specifiquement, quand une note est reellement declenchee,
  produit du bruit au lieu d'un son** -- confirme par test isole
  (piste 0 = DEXED, solo, note tenue -> "gros souffle pas de note").
- **ANALOG (onde simple), meme test isole, est confirme propre**
  ("la c'est bon") -- pas un probleme general du moteur audio/mixeur,
  specifique a DEXED.
- Le souffle "en silence total" du tout debut de la session s'explique
  par une note DEXED restee active suite a mes tests series du matin
  (STEP:/NOTE:/PLAY puis STOP) -- `stopSequencer()`/`allTrackNotesOff()`
  appelle bien `trackDexedEngine[track].keyup()`, mais quelque chose
  dans Synth_Dexed ne redescend pas franchement a zero (hypothese : 
  instabilite numerique de la boucle de feedback FM, type probleme de
  nombres denormaux -- classique en DSP, la sortie ne s'eteint jamais
  vraiment tant que le denormal persiste). Seul un power-cycle complet
  a nettoye l'etat -- pas rejouable a la demande avec juste STOP/PLAY.

**A faire (prochaine session, pas urgent pour continuer a tester)** :
creuser `src_teensy/microdexed-touch/third-party/Synth_Dexed/` pour la
vraie cause (verifier le traitement des denormaux dans la boucle de
feedback FM, comparer avec la reference MicroDexed-touch pour un
eventuel flag/optimisation manquant cote AZ-2). **En attendant, eviter
DEXED** -- EPIANO/BRAIDS/KARPLUS/ANALOG semblent utilisables (testes
individuellement sans souffle residuel, pas encore ecoutes en train de
vraiment jouer sauf ANALOG).

**[2026-09-18, suite -- corrige]** Retour utilisateur : "PLAY/STOP
marche pas, ca met en route le DEXED, faut reparer ce truc que ca
sonne plutot que ca fasse du bruit" -- PLAY/STOP fonctionnaient en
realite tres bien (confirme plus haut dans ce journal), le vrai souci
etait que DEXED demarre par defaut sur les pistes 0/1/4/5 (voir
`trackEngine[]` dans `src_teensy/az2_audio/main.cpp`), donc appuyer sur
PLAY sans rien changer jouait directement le moteur casse. Corrige :
**DEXED n'est plus le moteur par defaut d'aucune piste**, remplace par
ANALOG (seul moteur confirme propre a l'oreille) sur 0/1/4/5 ; EPIANO/
BRAIDS restent par defaut sur 2/3/6/7 (pas de bug signale dessus). DEXED
reste choisissable a la main via `ENGINE:` pour continuer a le tester/
le reparer plus tard -- juste plus le choix qui demarre tout seul.
Flashe et confirme vivant sur le vrai materiel (CPU 6.1%, plus bas
qu'avant -- coherent, ANALOG est moins couteux que DEXED).

Bugs UI corriges en route pendant cette session de flash (voir
commits) : navigation par pas au D-pad manquante (fix rate 1 : modif
via C, jamais teste car BTN:C n'a jamais genere d'evenement sur ce
montage physique -- fix rate 2 : roles HAUT/BAS <-> A inverses suite
au retour utilisateur, HAUT/BAS navigue par defaut, A maintenu edite),
et colonne NOTE qui n'affichait jamais rien tant que le pas etait OFF
(corrige : editer la note allume desormais le pas automatiquement).

**Signale, pas encore investigue** : bouton C ne genere jamais
d'evenement malgre plusieurs tests (cablage a verifier) ; page
SEQUENCEUR, le panneau "patch actif" ("cadre patch") signale comme peu
utilisable/rien ne se modifie dedans -- possiblement lie au
retrecissement du panneau (~194px -> ~106px) par les colonnes PRB/CND
ajoutees hier soir, a verifier a l'ecran.

**Demande produit notee pour plus tard** : patches "officiels" de
chaque moteur (vrais patchs DX7/EPiano/Braids) a integrer, edition de
patch, sauvegarde -- gros chantier, pas pour cette session.

**[2026-09-18 matin -- ESP32 flashe, les 2 cartes tournent ensemble]**
`pio run -e screen_esp -t upload --upload-port /dev/ttyUSB0` (CH340).
Boot propre : `DISPLAY:READY` (ecran VIEWE reel initialise),
`TOUCH:FT6336U:READY`, `SD:READY:size_mb=30429` (SD ESP32). **Liaison
serie ESP32<->Teensy confirmee en reel** -- tous les echos du Teensy
recus cote ESP32 (`TEENSY:HELLO:...`, `TEENSY:BPM:...`,
`TEENSY:ENGINE:...`x8, `TEENSY:PATTERN:0`, `TEENSY:STATUS:...:READY`).

**Souffle audio signale par l'utilisateur, apparu juste apres le flash
Teensy, present meme en silence total (aucune note jouee)** -- ecarte
une cause logicielle (rien dans le firmware ne touche a la chaine
audio/DAC cette session). Correspond exactement au symptome deja
documente dans `AZ2_DAC_PCM5102A.md` pour **SCK flottant sur le
PCM5102A** ("Flottant -> Souffle/bruit au lieu d'un son propre").
Cause probable : manipulation du Teensy pour le flasher (USB debranche/
rebranche) ayant deloge un fil/une soudure fragile SCK->GND. **A
verifier par l'utilisateur** : continuite SCK-GND au multimetre sur la
carte DAC, reconnecter/ressouder si mauvaise. Pas un bug de code.

**Reste a verifier a l'oeil** (ecran maintenant allume) : la page
SEQUENCEUR avec les 2 nouvelles colonnes PRB/CND (voir plus bas,
kTrkSideX retreci a ~106px pour le panneau "patch actif").

**[2026-09-18 matin -- premier flash reel de la session]** Teensy
flashe (`pio run -e master_teensy -t upload`, /dev/ttyACM0, HalfKay
detecte et programme sans probleme). Boot propre : SD dediee
(`SDTEENSY:READY:size_mb=59456`), PSRAM 16 Mo detectee + test lecture/
ecriture OK, sequenceur `tracks=8:steps=16`. Teste en serie
(pyserial) :
- `PROB:0:0:50`/`COND:0:0:33`/`FILL:1`/`FILL:0` -- acceptees et
  relayees normalement.
- `PROB:0:0:150` (hors bornes) -> `PROB:ERROR:OUT_OF_RANGE` ;
  `BPM:9999` (hors bornes) -> `BPM:ERROR:OUT_OF_RANGE` -- **premiere
  verification en reel du retour d'erreur ajoute hier**, fonctionne
  comme prevu.
- Pattern joue avec `SWING:127` (le nouveau maximum, plafonne a
  kTicksPerStep-2 depuis le fix d'hier) : les 16 pas s'enchainent sans
  accroc ni pas manquant, `CPU?` -> usage=11.6%/max=11.8%,
  memoire=133/141/200 blocs -- sain, coherent avec les mesures
  precedentes (8-15%).

**Confirme en reel pour la premiere fois** : le fix swing-max/CUT-
RETRIG, PROB:/COND:/FILL:, et le retour d'erreur protocole -- les 3
derniers points de l'audit du 2026-09-17, tous valides sur le vrai
Teensy. Reste a verifier : l'ecran ESP32 (pas encore branche a ce
stade de la session), donc les 2 nouvelles colonnes PRB/CND et le
retrecissement du panneau lateral.

**[2026-09-17, soir]** Nouvelle fonction tracker : **probabilite +
condition par pas** (PROB:/COND:/FILL:, voir AZ2_Protocol.h et
`SequencerTrack::stepProb/stepCondition` cote Teensy). Demandee par
l'utilisateur pour "faire un truc qui eclate tout" sur le tracker --
fonction la plus citee dans l'etude concurrence (AZ2_ETUDE_CONCURRENCE_
TRACKERS_GROOVEBOXES_2026.md) face a Elektron/Digitakt.

- Chaque pas ON garde son declenchement normal par defaut (prob=100%,
  cond=kStepCondAlways) -- **aucun changement de comportement sur un
  pattern existant** tant qu'on ne touche pas ces 2 nouveaux champs.
- Probabilite 0-100% : `random(100) < prob` a chaque passage, evalue
  dans `advanceTick()` (Teensy) avant de declencher le pas.
- Conditions "K sur N" (style Elektron, ex. 1:2/2:4/3:4) + FILL/!FILL,
  evaluees contre un compteur global de passages du pattern
  (`patternLoopCount`, incremente a chaque redemarrage de pattern) et
  un etat `fillActive` pilotable par `FILL:0/1`. Encodage sur un seul
  octet partage ESP32/Teensy (`az2::stepConditionEncode()`/
  `stepConditionMet()`/`stepConditionLabel()` dans AZ2_Protocol.h) pour
  ne pas dupliquer la logique.
- Cote ESP32 : 2 nouvelles colonnes tracker (PRB/CND), cycle
  `seqDetailCol` etendu de 4 a 6, echo `PROB:`/`COND:` mirrorees dans
  `seqStepProb[]`/`seqStepCondition[]`, format de sauvegarde projet
  etendu de 8 a 10 champs (retro-compatible : un ancien fichier a 8
  champs se relit avec prob=100/cond=0 par defaut).
- **PAS VERIFIE A L'ECRAN** (compile seulement, pas de materiel branche
  cette session) -- les 2 nouvelles colonnes retrecissent le panneau
  "patch actif" a droite du tracker de ~194px a ~106px (voir
  `kTrkSideX`/`kDetailProbW`/`kDetailCondW` dans `main.cpp`) : **premiere
  chose a regarder au prochain flash reel**, ajuster les largeurs si ca
  parait trop serre a l'oeil.
- Compile verifie pour les 3 environnements
  (`pio run -e master_teensy -e screen_esp -e ui_esp` -> SUCCESS).
- **BTN D = "fill" maintenu** -- **[2026-09-18 matin, corrige]** d'abord
  cable directement cote Teensy (`updateDigitalControls()`), mais en
  relisant le code au reveil : D a DEJA 2 sens existants selon l'ecran
  ESP32 affiche (page AUDIO : bascule clavier live/edition-pas ; page
  MOTEURS : bascule SOLO piste) -- le Teensy ne sachant PAS quel ecran
  est affiche, sa lecture directe de D aurait active "fill" en arriere-
  plan MEME en tenant D pour basculer solo/edition sur une autre page
  (silencieux tant qu'aucun pas n'a de condition FILL, mais un vrai
  piege des qu'on en programme un). Deplace cote ESP32 :
  `handleTeensyLine()` envoie `FILL:1` sur BTN:D:DOWN SEULEMENT si
  `currentScreen == Screen::Sequencer` (page ou D est libre), et
  `FILL:0` sur BTN:D:UP SANS condition d'ecran (pour ne jamais rester
  bloque a "actif" si on change de page en gardant D enfonce).
- `randomSeed(micros())` ajoute dans `setup()` Teensy (sinon `random()`
  rejoue exactement le meme motif a chaque mise sous tension).
- **Bug reel corrige** : `gbRecStart()` n'appelait pas `SD.remove()`
  avant `SD.open(path, FILE_WRITE)` -- sans consequence dans le cas
  normal (nextSampleName() trouve un nom LIBRE), mais dans le cas
  improbable ou les 999 noms `SAMPLE_NNN.wav` sont deja pris,
  `nextSampleName()` renvoie alors `SAMPLE_999.wav` en sachant qu'il
  existe deja -- `FILE_WRITE` sur SdFat OUVRE EN AJOUT sur un fichier
  existant (pas en ecrasement), donc la nouvelle capture se serait
  ajoutee APRES l'ancien contenu au lieu de le remplacer (wav corrompu/
  demesure). Note lors de l'audit du code du 2026-09-17 matin, deprioritise
  puis corrige ce soir (meme convention que savePatchSlot()/
  saveProject(), qui font deja ce remove()). Compile verifie.
- **Licence Synth_Braids resolue** : MIT confirme en lisant les entetes
  des fichiers vendored (Mutable Instruments, Emilie/Olivier Gillet) --
  voir AZ2_LICENCES.md, plus de point ouvert.
- **Premiers tests automatises AZ-2** (`env:native`, `test/
  test_protocol/`, voir "Tests et integration continue : absents"
  dans l'audit) : 9 tests unitaires sur la logique PURE de
  AZ2_Protocol.h (encodage PROB:/COND:, tables division/moteur/pad),
  tournent sur CETTE machine via `pio test -e native` (ArduinoFake
  fournit un Arduino.h factice, le header partage n'est pas modifie).
  `pio test -e native` -> 9/9 PASSED. Hors `default_envs`, n'affecte
  pas les 3 environnements materiels (recompiles avec succes juste
  apres pour confirmer).

**[2026-09-17, important pour toute future analyse externe]** `main`
etait reste fige au tout premier commit du projet (`b3eeae8`, 13
septembre -- Teensy = 134 lignes, un seul oscillateur sinus ; le
firmware ecran `src_esp32/az2_screen/` n'existait meme pas dessus)
pendant que ~140 commits de developpement reel se faisaient sur la
branche `az2-screen-engines-sequencer`. Un audit technique externe
("IA tierce") a ete lance contre `main` et a donc lu le code du jour 1
-- ses conclusions ("aucun sequenceur", "un seul moteur sinus", "aucune
interface ecran", "liaison non fonctionnelle") etaient vraies pour ce
commit-la mais fausses par rapport a l'etat reel du projet. `main` a
ete remis a jour le 2026-09-17 (fusion de `az2-screen-engines-
sequencer`, commit `419d110`) : les deux branches sont maintenant
alignees. **Toute IA/outil externe lance a l'avenir doit lire `main`
(ou `az2-screen-engines-sequencer`, identiques desormais) -- pas un
vieux commit** -- sinon le meme faux diagnostic se reproduira. Voir
`docs/AZ2_AUDIT_TECHNIQUE_COMPLET_2026-09-17.md`,
`AZ2_CIBLE_PRODUIT_ET_FEUILLE_DE_ROUTE_2026.md` et
`AZ2_ETUDE_CONCURRENCE_TRACKERS_GROOVEBOXES_2026.md` pour les 3
documents concernes -- leurs constats de code sont a relativiser pour
cette raison, mais leurs points d'hygiene de depot (pas de LICENSE
racine, pas de CI/tests, plateformes non figees, ~522 Mo dont 132 Mo
pour microdexed-touch) et leur vision produit (4 vues d'un meme
sequenceur, protocole verse/CRC/ack, etude concurrentielle M8/Polyend/
Elektron/etc.) restent valables et utiles independamment du bug de
branche.

**[2026-09-17, suite]** Chantier "hygiene du depot" demande par
l'utilisateur suite a l'audit externe ("on reste libre pour la licence
et on fait tout ce qu'il faut") :

- **Licence** : `LICENSE` (GPLv3) ajoute a la racine + inventaire complet
  dans `docs/AZ2_LICENCES.md` -- GPLv3 choisi parce que `Synth_MDA_EPiano`
  (moteur EPIANO, GPLv3 uniquement) est deja reellement compile dans le
  binaire `master_teensy`, ce n'etait donc pas un choix arbitraire.
  Point ouvert note dans ce doc : la licence de `Synth_Braids` n'est pas
  documentee dans ce vendoring, a verifier avant toute distribution
  binaire plus large.
- **Plateformes PlatformIO figees** : `platform = teensy` ->
  `teensy@6.0.0`, et les deux environnements ESP32 pointent maintenant
  explicitement vers le meme tag de release pioarduino
  (`55.03.311`) au lieu de l'alias mouvant `stable` (ou de
  `espressif32` nu, qui resolvait silencieusement vers ce meme fork
  installe localement -- pas reproductible tel quel sur une autre
  machine).
- **Dependance morte retiree** : `lvgl` (declaree dans `screen_esp`
  depuis le debut du projet, jamais utilisee -- 0 reference a `lv_`/
  `lvgl.h` dans `az2_screen/`, le menu/tracker sont dessines a la main
  via GFX Library for Arduino) + `include/lv_conf.h` (784 lignes,
  meme sort).
- **~89 Mo de bloat vendored supprimes** : 2 manuels PDF de
  MicroDexed-touch (66+3.2 Mo, doc de l'UI d'origine qu'on ne garde pas)
  et `drumsamples.h` (20 Mo, jamais compile). `retro-go-master`/
  `Launcher_lvgl-master` etaient deja supprimes avant.
- **CI ajoutee** (`.github/workflows/build.yml`) : compile les 3
  environnements (`master_teensy`, `screen_esp`, `ui_esp`) a chaque push/
  PR sur GitHub Actions. Ne remplace pas un test materiel, mais
  detecte automatiquement une regression de compilation au lieu de la
  decouvrir des semaines plus tard.

Compile verifie en local pour les 3 environnements apres tous ces
changements (`pio run -e master_teensy -e screen_esp -e ui_esp` ->
SUCCESS) avant de pousser.

**[Rafraichi le 2026-09-17]** -- ce document a commence comme un topo
ponctuel (2026-09-15) puis est devenu un journal chronologique (sections
datees plus bas, gardees pour l'historique). Les 3 sections qui suivent
resument l'etat REEL actuel ; pour le detail jour par jour, voir le
journal en dessous de "Journal de la nuit".

## Vue d'ensemble

AZ-2 est une groovebox modulaire a 2 cartes, orientee Game Boy/GBC :
- **Teensy 4.1** (`src_teensy/az2_audio/`) : moteur audio temps reel
  (5 synthes, filtre/ADSR/volume/mute/solo par piste), sequenceur/
  tracker, sampler (capture), MIDI IN, DAC I2S (PCM5102A), carte SD
  dediee (samples).
- **ESP32-S3** (`src_esp32/az2_screen/`) : ecran tactile 480x480 RGB
  parallele (carte VIEWE UEDX48480040E-WB), interface utilisateur,
  emulateur Game Boy/GBC, carte SD (ROMs, patches, projets).
- Liaison UART 230400 bauds entre les deux (Serial1 Teensy <->
  GPIO19/20 ESP32), protocole texte ligne par ligne + paquets binaires
  (audio GB, oscilloscope) -- partage via `lib/AZ2_Protocol/`.
- Croix + 4 boutons + 3 encodeurs rotatifs (avec bouton) cables
  directement sur le Teensy -- remplacent le Pico/matrice abandonnes
  le 2026-09-14.

## Ce qui marche et est CONFIRME en reel (verifie sur le vrai materiel)

- **Son** : 8 pistes, 5 moteurs, patch reel par moteur, filtre resonant
  par piste, ADSR par piste, bus d'effets maitre (reverb+delay), sortie
  DAC -- confirme audible.
- **Ecran** : affichage + tactile multi-doigt.
- **Lien UART** Teensy<->ESP32 (HELLO, echo d'etat complet).
- **Croix + boutons A/B/C/D + 3 encodeurs (bouton integre)** : cables
  et testes en reel.
- **Emulateur Game Boy/GBC** (Walnut-CGB) : ROM reelle testee (Tobu
  Tobu Girl), sauvegarde cart RAM sur SD.
- **Sampler, capture (phase 1)** : ecriture sur la carte SD DEDIEE du
  Teensy confirmee (`REC:START`/`REC:STOP` testes en serie).
- **Reglages Dexed (DXP:, algo/feedback)** : testes en serie.
- **Menu ecran** 4 categories, navigable croix+A/B+tactile.
- **Ecran de veille** ("matrix"), configurable.
- **Toolchain** `teensy_loader_cli` recompile depuis la source
  officielle -- fiable, plusieurs flashs reussis.

## Ce qui est CODE mais PAS ENCORE VERIFIE EN REEL

Tout ce qui suit compile mais n'a jamais ete flashe/entendu/vu depuis
son ecriture (la session du 2026-09-17 a ete faite sans aucun board
branche) -- **priorite au reveil du materiel : tout reverifier avant
d'ajouter quoi que ce soit d'autre** :

- **Mute/solo par piste** (boutons C/D, page MOTEURS).
- **Volume par piste** (page PATCH).
- **Sauvegarde/chargement de PROJET complet** (page PROJET).
- **MIDI notes IN** (jamais teste avec un vrai controleur).
- **Clavier tactile comme editeur live** (bouton D, page AUDIO).
- **Pagination de la liste de ROM** (16->40, defilement par pages de 8).
- **Bouton C pour quitter une partie GB**.
- Tracker : ticks/colonnes NOTE-INST-FX-VAL/chainage song/gammes --
  flashes et utilises depuis, stables.

## Bugs connus / limites actuelles

- Colonne **INST** (patch par pas) : stockee et transmise, **pas
  encore appliquee** au son reel (changer de patch Dexed est trop
  couteux pour tourner dans l'ISR du sequenceur).
- **Accords** (plusieurs notes par pas) : pas fait, plus gros que
  prevu -- voir AZ2_BENCHMARK_CONCURRENCE.md.
- **Swing/groove** : pas fait, piege timer/ISR trouve -- voir
  AZ2_FEUILLE_DE_ROUTE.md.
- **Sampler** : capture faite, PAS de lecture (pas de moteur
  `AudioPlaySdWav`/`AudioPlaySdRaw`), pas de vue d'onde/decoupage/
  nommage a l'ecran.
- **Pas de vrai panoramique** (chaine audio mono de bout en bout).
- **GitHub** : remote configure mais authentification pas encore
  etablie sur cette machine -- voir la section GitHub du journal.

## Journal de la nuit -- ce qui a ete fait

Ordre reel (pas tout a fait celui prevu au depart, un bug reel a pris
du temps a chasser -- voir plus bas) :

1. **[FAIT, teste en reel]** Couleurs par effet sur le tracker (grille
   + vue detail) -- une couleur par effet (ARP/CUT/RET), pastille sur
   la grille, fond teinte sur la vue detail. Compile, pas teste a
   l'oeil (ESP32 pas flashe cette nuit).
2. **[FAIT, teste en reel]** Filtre resonant par piste (`FILT:`,
   `AudioFilterStateVariable` -- Chamberlin SVF entier, tres bon
   marche en CPU). Insere entre chaque moteur et son mixeur de groupe.
   Teensy flashe, `CPU?` interroge : 8.6%/9.9% -- essentiellement
   identique a avant (8-15% deja mesure), les 8 filtres n'ajoutent
   presque rien. Grand ouvert par defaut (le constructeur de la lib
   partait a 1000Hz, corrige explicitement sinon toutes les pistes
   auraient ete etouffees des le premier boot).
3. **[FAIT, teste en reel]** ADSR editable par piste (`ENV:`), applique
   a `trackAnalogEnv[]` (seul moteur de la palette avec une vraie
   enveloppe generique exposable -- Dexed/EPiano ont leur propre
   comportement interne au patch, Braids/Karplus n'ont pas
   d'enveloppe). Teste : commande acceptee/relayee, CPU inchange.
4. **[FAIT, BUG REEL trouve et corrige, teste en reel]**
   Oscilloscope Teensy -> ESP32 (`SCOPE:`, meme principe de paquet
   binaire que le son GB mais dans l'autre sens). Premiere version
   **gelait completement le Teensy** des que PLAY et SCOPE tournaient
   en meme temps (plus aucune reponse serie du tout). Diagnostique
   methodiquement par bissection sur les commits precedents (tick
   rewrite seul OK, filtre seul OK, ADSR seul OK -- donc le bug etait
   forcement dans le scope non commite), puis isolation precise
   (connect() seul OK, begin() seul OK, les deux + audio reel qui
   circule = gel garanti). Cause : la boucle de purge appelait
   `freeBuffer()` SANS `readBuffer()` d'abord -- `freeBuffer()` ne fait
   RIEN si aucun bloc n'a ete lu, donc la boucle tournait a l'infini.
   Corrige, reteste plusieurs fois avec le sequenceur actif 5s+ en
   continu : CPU 10.6%, memoire audio saine (135-143/200 blocs), aucun
   gel. Cote ESP32 : page PATCH (selecteur de piste, tracer d'onde,
   6 reglages CUTOFF/RESONANCE/ATTACK/DECAY/SUSTAIN/RELEASE) codee et
   compilee, **jamais vue a l'ecran** (ESP32 pas flashe).
5. **[FAIT]** Gammes (verrouillage a la saisie) : 5 gammes (chromatique/
   majeur/mineur/pentatoniques, racine C fixe), reglage sur la page
   CONFIGURATION. `nextNoteInScale()` remplace le "+/-1 demi-ton" sur
   la croix (grille ET vue detail du sequenceur) -- saute a la
   prochaine note dans la gamme. Chromatique = comportement d'origine,
   retro-compatible par defaut. Cote ESP32 uniquement, rien cote
   Teensy. Compile, pas vu en reel.

Verification finale (fin de nuit) : `pio run -e master_teensy -e
screen_esp` -- les deux compilent ensemble sans erreur. Teensy
reinterroge une derniere fois : `CPU?` -> usage=9.4%/max=9.5%, memoire
132-144/200 blocs -- stable sur toute la nuit de tests.

## Etat au reveil -- ce qui reste

- **ESP32 jamais reflashe cette nuit** (mode boot manuel requis, acces
  physique). Tout ce qui est cote ecran depuis "Tracker : ticks
  internes..." (voir git log) n'a ete verifie qu'a la compilation :
  fix 8 pistes, vue detail tracker, couleurs d'effets, son GB au DAC
  (partie ESP32 qui envoie), page PATCH + oscilloscope, gammes. **Rien
  de tout ca n'a ete vu a l'oeil ni entendu.**
- **Teensy a jour et verifie en reel** pour tout : tick rewrite, filtre,
  ADSR, oscilloscope (bug corrige).
- Prochaine chose utile au reveil : mode boot manuel sur l'ESP32
  (BOOT maintenu + RESET + relacher BOOT), puis `pio run -e screen_esp
  -t upload --upload-port /dev/ttyUSB0`, puis verifier a l'oeil/a
  l'oreille tout ce qui precede.
- Pas commence : gammes/accords, clavier tactile comme editeur live,
  sauvegarde/chargement de projet (etapes 5-7 de
  `AZ2_TRACKER_ETUDE.md`), colonne INST reellement appliquee au son,
  chainage de patterns, role "gachette" pour C/D.

## Mise a jour 2026-09-16

Suite reflex demandee sur le panneau lateral du sequenceur ("on a acces
aux reglages du patch ? ... un bouton pour agrandir ... on doit pouvoir
sauvegarder les patchs ... un sampleur ... a integrer") :

- **Panneau "patch actif"** (`drawTrkSidePanel()`) : affiche desormais
  CUTOFF/RESONANCE + ADSR (ou ALGO/FEEDBACK pour une piste DEXED, voir
  plus bas) de la piste selectionnee, avec un bouton **AGRANDIR** qui
  ouvre la page PATCH complete pour cette piste.
- **Sauvegarde/chargement de patch** : page PATCH, ligne SLOT (0-7) +
  boutons SAVE/LOAD. Ecrit sur la carte SD de l'ESP32 dans
  `/patches/N.txt` (moteur, patch, cutoff, reso, ADSR, + algo/feedback
  Dexed). LOAD renvoie ENGINE:/PATCH:/FILT:/ENV:/DXP: au Teensy.
  **ESP32 reflashe et boot verifie propre en reel** (lien Teensy
  toujours vivant) ; **pas teste manuellement** (SAVE/LOAD pas encore
  touches a l'ecran par un humain).
- **Reglages propres a DEXED** (`DXP:`, demande "on n'a pas de reglages
  dans la fenetre dexed du tracker") : ADSR generique n'a jamais eu
  d'effet sur ce moteur (sa propre EG DX7 la remplace) -- page PATCH
  affiche maintenant ALGORITHME (1-32) et FEEDBACK (0-7) a la place pour
  une piste Dexed, lignes 4-5 grisees ("sans effet sur ce moteur").
  **Cote Teensy (`handleDexedParamCommand`) compile mais PAS FLASHE** --
  le Teensy n'etait pas accessible en USB depuis cette machine au
  moment du dev (relie a l'ESP32 par l'UART Serial1 seulement, ce qui
  explique le lien "TEENSY_AUDIO:READY" toujours vu au reboot ESP32).
  **[2026-09-17, confirme]** carte SD Teensy (`BUILTIN_SDCARD`) **PRESENTE
  et fonctionnelle** -- teste directement en serie (`REC:START` ->
  `REC:STARTED:/samples/SAMPLE_001.wav`, `REC:STOP` ->
  `REC:STOPPED:samples=0`, fichier cree avec succes). Plus de blocage
  materiel pour le sampleur -- reste a verifier le contenu audio reel
  (jouer un jeu pendant l'enregistrement, ecouter le .wav resultant).
  **[2026-09-17] Flashe et verifie en reel** : `CPU:usage=8.6%:max=9.9%`,
  `MEM:blocks=133:max=141/200` (identique aux mesures d'avant, aucune
  regression). `DXP:0:0:5`/`DXP:0:1:3` testes en direct par serie USB --
  bien relayes/appliques. Rechargement de patch (`PATCH:0:0`) confirme
  ecraser l'algo/feedback avec ceux de la banque (`DXP:0:0:4`/
  `DXP:0:1:0`), comportement voulu et documente.
- **Page MOTEURS, navigation croix** (demande "j'ai pas le controle
  joystick pour choisir et regler les moteurs") : HAUT/BAS choisissent
  la piste (surlignage blanc), GAUCHE/DROITE changent la valeur de la
  colonne au focus, BTN:A bascule le focus MOTEUR/PATCH. **ESP32
  reflashe, boot verifie propre**, comportement du tactile inchange
  (toujours utilisable en parallele).
- **Sampleur (moteur audio)** : PAS commence. Bloque tant qu'on n'a pas
  confirme une carte SD reellement presente dans le lecteur
  `BUILTIN_SDCARD` du Teensy (dernier statut connu : `SDTEENSY:
  NOT_PRESENT`) -- aucun fichier son n'existe encore, et un nouveau
  type de moteur (`AudioPlaySdWav`/`AudioPlaySdRaw`) + protocole de
  selection d'echantillon restent a concevoir. **Carte micro SD 58 Go
  preparee en FAT32 le 2026-09-16** (label `AZ2SAMPLES`, dossier
  `/samples/`) -- reste a l'inserer physiquement dans le lecteur du
  Teensy et confirmer `SDTEENSY:READY`.

## Journee du 2026-09-16 -- audit + recherche (utilisateur absent)

Demande : "tu as la journee pour faire des recherches sur nos
concurrents, liste des ameliorations indispensables, etat des lieux
logiciel/firmware, on optimise vitesse, on ameliore l'affichage, on
fait un job sur tout code mort, audit de code, mise a jour du GitHub,
presentation du projet et documentation". Boards absents (ni Teensy ni
ESP32 sur le bus USB de cette machine) -- tout ce qui suit est
**compile-verifie uniquement**, rien vu/entendu sur le vrai materiel
aujourd'hui.

- **Audit de code mort** : supprime `src_pico/` (Pico abandonne,
  cablage deja documente en prose ailleurs), `src_esp32/
  Launcher_lvgl-master/` (75 Mo vendores d'un exemple fournisseur,
  jamais reference dans le build), les stubs vides `src_esp32/main.cpp`
  et `src_teensy/main.cpp`, et `kHelloKeypad` (protocole) devenu
  orphelin. Verifie par grep avant suppression a chaque fois (rien
  d'autre ne referencait ces fichiers), et recompilation des 3
  environnements (`master_teensy`, `screen_esp`, `ui_esp`) apres coup.
  `src_esp32/az2_control/` (env `ui_esp`) et le gros dossier `src_teensy/
  microdexed-touch/` sont volontairement GARDES (reference active/
  bring-up isole documente comme tel, pas du code mort au meme sens que
  le Pico).
- **Petit bug UX corrige** : page MOTEURS, un toucher tactile ne
  deplacait pas le curseur croix -- desynchronisation possible entre
  tactile et croix. Corrige + recompile.
- **Recherche concurrence** : 2 machines ajoutees au comparatif
  (Teenage Engineering OP-XY, nanoloop) + 2 reverifiees (M8 Model:02,
  Polyend Tracker Mini) -- voir AZ2_BENCHMARK_CONCURRENCE.md pour le
  detail et les sources. Liste d'ameliorations indispensables priorisee
  ajoutee (mute/solo, sauvegarde de projet complet, swing, volume/pan
  par piste, sampler, MIDI, accords, clavier live).
- **Feuille de route** : ajout d'une "Phase 8" qui reprend cette liste
  priorisee, plus un tableau qui fait correspondre les vieilles phases
  0-7 (matrice/Pico/Retro-Go, plus d'actualite) a ce qui existe
  reellement aujourd'hui.
- **README.md** : entierement reecrit -- decrivait encore la matrice
  SparkFun 4x4 et le Pico comme actifs, en anglais, sans lien vers la
  moitie des docs recentes (tracker, etat des lieux, cablage master).
- **Performance** : revue du code (Teensy hot-path, boucles de dessin
  ESP32) -- pas de probleme flagrant trouve. Les redessins sont deja
  partiels (ligne par ligne sur MOTEURS/PATCH/sequenceur, tracer
  d'oscilloscope decime+limite en frequence). Usage de `String` pour le
  parsing des commandes serie (cote Teensy et ESP32) : design deja en
  place depuis le debut du projet, fonctionne dans la marge CPU/memoire
  mesuree (~9-10%, 130-144/200 blocs audio) -- **pas touche** sans
  materiel pour verifier un changement aussi central sans risque de
  regression invisible tant que personne ne peut tester en reel.
- **GitHub** : **bloque**. Le remote `origin` (github.com/propann/
  L-AZ-2) est configure mais aucune methode d'authentification
  disponible sur cette machine (pas de `gh auth login`, pas de jeton
  `GH_TOKEN`, pas de credential helper git) -- `git push` echoue avec
  "could not read Username". La branche `az2-screen-engines-sequencer`
  est 33+ commits devant `origin/main`, jamais poussee. Necessite une
  action de l'utilisateur (`gh auth login` ou jeton personnel) pour
  debloquer.

## Journee du 2026-09-17 -- emulateur GB + sampler phase 1

- **Verification du mapping manette GB** : Start/Select (encodeurs 1/2)
  confirmes deja en place depuis le 2026-09-15. Tous les boutons d'une
  vraie Game Boy couverts.
- **[FAIT, flashe, teste en reel]** Bouton **C** pour quitter une
  partie en cours (`goTo(Screen::Menu)`, sauvegarde la RAM cartouche).
  Page A PROPOS mise a jour avec le mapping complet.
- **[FAIT, flashe]** Pagination de la liste de ROM : `kGbMaxRoms` 16 ->
  40, affichage par pages de 8 avec fleches </> -- suite a l'ajout de
  34 jeux perso sur la carte SD ESP32 (Zelda, Mario, Tetris, Pokemon,
  Kirby, Wario, etc., copies depuis une cle USB personnelle, PAS dans
  le depot git).
- **[FAIT, flashe, PARTIELLEMENT teste]** Sampler GB phase 1
  (REC/STOP) : encodeur 0 declenche l'enregistrement, capture le son
  GB natif (8kHz) dans un `.wav` sur la SD DEDIEE du Teensy. **Ecriture
  SD confirmee en reel** (`REC:START`/`REC:STOP` testes directement en
  serie, fichier cree avec succes). **Pas encore teste avec un vrai
  jeu qui joue du son** (0 echantillon capture lors du test, aucune ROM
  chargee a ce moment) -- prochaine etape : enregistrer pendant une
  vraie partie et ecouter le fichier resultant.
- Phase 2/3 du sampler (vue d'onde en direct, decoupage tactile,
  decoupage automatique, clavier de nom, moteur de lecture) : pas
  commencees, detaillees dans AZ2_FEUILLE_DE_ROUTE.md.

## Teensy reflashe et VERIFIE EN REEL (2026-09-17, soir)

Tout le paquet du jour (mute/solo, volume, sauvegarde de projet, MIDI
IN, swing, sampler) flashe sur le Teensy et teste directement en
serie :

- Boot propre, `CPU:usage=8.6%:max=9.9%`, `MEM:blocks=133:max=141/200`
  -- identique a avant tous ces ajouts, aucune regression.
- `SWING:64`, `MUTE:0:1/0`, `SOLO:0:1/0`, `VOL:0:100` : tous acceptes
  et relayes correctement.
- **Test critique swing** : `SWING:96` (quasi maximum) puis `PLAY`
  pendant 5 secondes -- le sequenceur a avance plusieurs mesures sans
  aucun gel, CPU/memoire strictement identiques a l'avant. Tempo moyen
  visuellement correct (progression des CLOCK: coherente avec le BPM
  malgre l'asymetrie forte des pas). **Le mecanisme "ticksForCurrentStep
  variable, timer jamais reconfigure" tient la route en reel.**
- `REC:START`/`REC:STOP` (sampler) toujours fonctionnel apres tous ces
  ajouts (`SAMPLE_002.wav` cree).

**Mute/solo sur une piste Braids en train de jouer une note tenue**
(le cas precis que le piege d'origine aurait casse) : PAS ENCORE testE
manuellement (demande de jouer une note et de mute/demute pendant
qu'elle sonne, pas juste d'envoyer les commandes a vide) -- a faire au
prochain test avec le son.

## Audit complet du code (2026-09-17)

Relecture ligne par ligne des 2 firmwares + protocole (~6800 lignes)
demandee par l'utilisateur pour recouper avec des analyses d'autres IA.
3 corrections rapides appliquees (sans risque, pas besoin de materiel) :

- **Message trompeur a l'ecran** : la page JEUX sans ROM disait encore
  "Pas de son pour l'instant" -- corrige (le son GB fonctionne depuis
  longtemps).
- **Code mort** : `kRecToggle` (AZ2_Protocol.h) jamais utilise nulle
  part, decrivait en plus une convention ("toggle") differente de celle
  reellement implementee (REC:START/REC:STOP explicites) -- supprime.
- **Commentaire trompeur** : `handlePatternCommand()` disait qu'un
  changement de pattern edite est "effectif au prochain pas" -- en
  realite `advanceTick()` ne bascule qu'au prochain redemarrage de
  pattern (jusqu'a 16 pas plus tard), pas au pas suivant. Comportement
  voulu, juste le commentaire corrige.

3 points restants, notes pour plus tard (pas de risque immediat) :
setup() ESP32 s'arrete net si l'ecran ne s'initialise pas (Teensy/SD
jamais inities dans ce cas) ; aucune commande Teensy hors bornes ne
renvoie d'erreur (silencieusement ignoree) ; a swing maximum, les pas
raccourcis (1 tick) ne laissent plus de fenetre aux effets CUT/RETRIG.

**2026-09-17 (suite) -- 1er point corrige** : `setup()` ESP32 continue
maintenant Serial1 (lien Teensy)/tactile/SD meme si `gfx->begin()`
echoue -- seuls l'intro et le premier affichage (qui ont besoin de
l'ecran) sont sautes. Avant, un ecran en panne coupait aussi tout
diagnostic serie du Teensy/tactile/SD, avec une seule ligne
`DISPLAY:ERROR:BEGIN_FAILED` comme unique symptome. Compile verifie
(`pio run -e screen_esp`) ; a verifier sur materiel reel le jour ou
l'ecran a un vrai probleme d'init (pas teste, pas facile a provoquer
sans debrancher l'ecran expres).

**2026-09-17 (suite) -- 2e point corrige (retour d'erreur cote
protocole)** : les ~22 handlers de commandes texte cote Teensy
(STEP/NOTE/INST/SFX/PATTERN/SONGSET/SONGLEN/SONGMODE/BPM/DIV/SWING/
ENGINE/PATCH/FX/VOL/MUTE/SOLO/FILT/ENV/DXP/SCOPE/PAD/MACRO) qui
ignoraient silencieusement une commande malformee ou hors bornes
envoient maintenant `<COMMANDE>:ERROR:<raison>` (raison =
`MALFORMED`, `OUT_OF_RANGE` ou `UNKNOWN_PARAM`/`UNKNOWN_INDEX` selon le
cas) sur Serial (USB) ET Serial1 (vers l'ESP32) via le nouveau helper
`sendCommandError()`. Reprend la convention deja existante pour
`REC:ERROR:` (gbRecStart()), ne l'invente pas. L'ESP32 ne traite pas
encore ces lignes specifiquement (il ignore deja les lignes non
reconnues, donc rien ne casse), mais elles sont visibles au moins sur
le moniteur serie USB du Teensy des maintenant -- suffisant pour le
diagnostic manuel/les recoupements d'analyse en cours. Compile verifie
(`pio run -e master_teensy`) ; pas de changement de comportement sur
le chemin valide (memes conditions de validation qu'avant, juste un
message en plus quand ca echoue) -- a confirmer sur materiel avec de
vraies commandes hors bornes envoyees depuis l'ESP32/un script de test.
SWING:/SONGMODE:/MACRO: n'ont qu'une erreur "MALFORMED" (ligne sans le
bon nombre de `:`) : SWING clampe deja sa valeur (comportement voulu,
pas une erreur), SONGMODE n'a pas de plage a valider (0/tout le reste
= false), MACRO n'agit que sur l'index 1 pour l'instant (les autres
index sont "pas encore cable", pas "invalide").

**2026-09-17 (suite) -- 3e point corrige (fenetre CUT/RETRIG a swing
max)** : confirme le mecanisme exact du bug. `triggerStepFx()` (ARP/
CUT/RETRIG) n'est appelee QUE dans la branche `else` (`currentTick !=
0`) d'`advanceTick()`. Avec l'ancien plafond (`swingAmount` jusqu'a
`kTicksPerStep-1` = 3), un pas pair au swing maximum tombait a
`ticksForCurrentStep = 1` : `currentTick` valait alors toujours 0
pendant tout le pas, donc cette branche `else` n'etait JAMAIS executee
-- un CUT ou un RETRIG pose sur ce pas ne se declenchait plus du tout
(le son jouait entier, coupe seulement par l'`allTrackNotesOff()`
normal du pas suivant). Fix : plafonne `swingAmount` a
`kTicksPerStep-2` (jamais moins de 2 ticks par pas) dans
`handleSwingCommand()` -- garantit au moins UN passage dans la branche
`else` par pas, donc au moins une chance pour CUT/RETRIG de s'appliquer
meme sur le pas le plus raccourci. Cout : perd le tout dernier cran de
swing le plus extreme (127/127 sur l'echelle UI) -- c'etait justement
celui qui cassait CUT/RETRIG, aucun autre effet de bord attendu.
Compile verifie (`pio run -e master_teensy`) ; **a confirmer a
l'oreille sur materiel reel** (poser un CUT ou un RETRIG serre sur un
pas pair, monter le swing au maximum, verifier que l'effet continue de
s'entendre au lieu de disparaitre).

Piste specifiquement verifiee et ECARTEE : `frame_skip` (Walnut-CGB)
pourrait desynchroniser l'audio de la vitesse du jeu -- verifie dans le
code source (`walnut_cgb.h`, VBlank pose `gb->gb_frame=true` a CHAQUE
frame reelle quel que soit `frame_skip`, qui ne saute QUE le rendu
pixel) : pas de bug la-dedans, confirme.

### Menage supplementaire : mentions du pad 4x4 (demande separee)

Passage dedie sur toutes les mentions de la matrice SparkFun 4x4/Pico
(hardware abandonne le 2026-09-14) pour verifier qu'aucune ne pretend a
tort etre le hardware actuel :

- Code et docs actifs (`main.cpp` ESP32/Teensy, `AZ2_Protocol.h`,
  `README.md`, `AZ2_CABLAGE_MASTER.md`, `platformio.ini`) : deja propres
  ou deja correctement marques "ABANDONNE" -- rien a faire.
- `AZ2_FEUILLE_DE_ROUTE.md` : la section "Phase 3 - Matrice SparkFun
  4x4" (plan d'origine) n'etait annotee comme abandonnee que bien plus
  bas dans le document (section "Mise a jour 2026-09-16") -- un lecteur
  qui lit dans l'ordre pouvait la croire encore active. Ajoute un
  avertissement direct sur la section elle-meme.
- `AZ2_PORTAGE_MICRODEXED_TOUCH.md` : contrairement aux autres docs de
  planification precoce (ECRAN_FACADE, ARCHITECTURE_FIRMWARE_DOUBLE,
  ESP32_CONTROLE_WIFI_SD_RETRO), celui-ci n'avait PAS recu de note
  "perime" alors qu'il presente la matrice 4x4 comme le controle retenu
  -- note ajoutee en tete de fichier.
- `src_esp32/az2_control/main.cpp` (environnement `ui_esp`, garde comme
  reference isolee, hors `default_envs`) : implemente reellement le
  scan mux 4x4 (`AZ2:FEATURE:SPARKFUN_4X4_MATRIX` etc.), donc le code
  n'est pas faux en soi, mais rien dans le fichier ne disait que ce
  hardware est abandonne et que ce n'est pas le firmware ESP32 shippe --
  commentaire d'entete ajoute pour clarifier. Compile verifie
  (`pio run -e ui_esp`), pas de changement de comportement.

## Bugs reels trouves en jouant (2026-09-17, utilisateur de retour)

Premier vrai test manette en main apres le flash de l'ESP32 -- 2 bugs
reels rapportes et corriges dans la foulee :

1. **"je peux pas selectionner une ROM avec la croix et A/B, avoir le
   nom en surbrillance"** -- la croix sur la page JEUX etait TOUJOURS
   routee vers `gbSetButton()` (boutons du jeu), meme quand aucune ROM
   n'etait encore chargee : aucune navigation clavier n'atteignait
   jamais la liste. Corrige : `selectedRomIndex` + surbrillance (meme
   convention que la piste choisie sur la page MOTEURS), HAUT/BAS
   deplacent la selection (suit le defilement automatiquement), A
   charge la ROM selectionnee. `gbSetButton()`/le mapping A/B ne
   s'activent plus que si une partie est REELLEMENT en cours
   (`gbIsLoaded()`), pas juste "page JEUX affichee".
2. **"bug d'affichage quand ca commence a demarrer l'emulation"** --
   trouve : `drawLinkStatus()` (bandeau "TEENSY: relie" en bas de
   l'ecran, `kStatusY = 450`) chevauche le bas de l'image du jeu
   (celle-ci va de y=24 a y=456, voir `gbBlitLine()`). Cette fonction
   est appelee a CHAQUE ligne recue du Teensy, y compris `STATUS:`
   envoye ~1x/seconde en continu -- une barre noire + texte
   s'incrustait donc sur le bas de l'ecran de jeu toutes les secondes
   pendant qu'une partie tournait. Corrige : exclue quand
   `currentScreen==Screen::Retro && gbIsLoaded()`, meme famille
   d'exclusion que Controls/Links deja en place.

Compile verifie, **pas encore reflashe** au moment d'ecrire cette
entree -- ESP32 deconnecte entre le rapport de bug et le fix.

## Suite du 2026-09-17 -- 5 des 8 priorites indispensables faites

Utilisateur parti travailler ("on attaque la feuille de route, avance
un max"). Boards absents de cette machine tout le long -- **tout ce qui
suit est compile-verifie uniquement**, rien vu/entendu sur le vrai
materiel :

1. **Mute/solo par piste [FAIT]** -- le piege Braids (gain de groupe =
   porte note-on/off) resolu proprement via `trackNoteHeld[]` +
   `applyGroupGainNow()` : mute/demute immediat meme sur une note
   Braids tenue. Boutons C/D (page MOTEURS).
2. **Sauvegarde/chargement de projet complet [FAIT]** -- page PROJET,
   4 emplacements, patterns+song+tempo+gamme+moteurs+mute (pas solo).
   Purement ESP32, zero risque audio.
3. Swing/groove -- **pas fait** (piege timer trouve, voir feuille de
   route).
4. **Volume par piste [FAIT]** -- meme mecanisme que mute/solo (memes
   fonctions `applyGroupGainNow()`/`trackEffectiveGain()`). Pan pas
   fait (chaine mono de bout en bout).
5. Sampler -- **phase 1 confirmee en reel hier** (ecriture SD testee),
   phases 2/3 (vue d'onde, decoupage, nommage) pas commencees.
6. **MIDI notes IN [FAIT]** -- MVP, route vers la voix live
   (`liveVoice`), `USB_MIDI_SERIAL` dormait dans platformio.ini depuis
   le debut du projet. Pas de synchro horloge, pas de MIDI OUT.
7. Accords -- **pas fait** (plus gros que prevu, touche le meme code
   que le bug de gel deja rencontre cette session).
8. **Clavier tactile comme editeur live [FAIT]** -- bouton D (page
   AUDIO) bascule "jouer en direct"/"poser sur le pas selectionne".

Reste vraiment a faire, dans l'ordre : tester TOUT ce qui precede en
reel des que les boards sont disponibles (rien n'a ete verifie
aujourd'hui, seulement compile), puis swing et accords (les deux
demandent du soin sur le vrai materiel a cause des pieges trouves),
puis le sampler phase 2/3 (design UI a faire avec l'ecran sous les
yeux).
