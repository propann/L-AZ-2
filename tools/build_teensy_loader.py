# AZ-2 - Recompile teensy_loader_cli depuis la source officielle a jour
# (voir tools/teensy_loader_cli/README.md pour le pourquoi -- le binaire
# 2.2 fourni par le paquet PlatformIO "tool-teensy" a un vrai bug de
# parsing .hex sur les gros firmwares Teensy 4.1, trouve le 2026-09-15
# des l'ajout de la carte SD cote Teensy pour le sampleur). Charge via
# `extra_scripts` de l'environnement master_teensy dans platformio.ini.
#
# Compile une seule fois dans .pio/local_tools/ (pas commite au depot,
# recree si absent ou si la source a change), puis redirige UPLOADER
# vers ce binaire frais au lieu de celui de PlatformIO.
Import("env")

import os
import subprocess

project_dir = env["PROJECT_DIR"]
src_c = os.path.join(project_dir, "tools", "teensy_loader_cli", "teensy_loader_cli.c")
out_dir = os.path.join(project_dir, ".pio", "local_tools")
out_bin = os.path.join(out_dir, "teensy_loader_cli")


def ensure_built():
    os.makedirs(out_dir, exist_ok=True)
    needs_build = (
        not os.path.exists(out_bin)
        or os.path.getmtime(src_c) > os.path.getmtime(out_bin)
    )
    if needs_build:
        print("AZ2: compilation de teensy_loader_cli (source officielle a jour, voir tools/teensy_loader_cli/README.md)")
        subprocess.run(
            ["gcc", "-O2", "-Wall", "-DUSE_LIBUSB", "-o", out_bin, src_c, "-lusb"],
            check=True,
        )
        os.chmod(out_bin, 0o755)


def use_local_uploader(source, target, env):
    # env.Replace(UPLOADER=...) fait tot (script "pre") est ensuite
    # ecrase par le SConscript teensy-cli du framework, qui fixe
    # UPLOADER="teensy_loader_cli" en configurant le protocole d'upload
    # -- il faut donc re-forcer UPLOADER juste avant l'action d'upload
    # elle-meme (AddPreAction sur la cible "upload"), pas plus tot.
    try:
        ensure_built()
        env.Replace(UPLOADER=out_bin)
        print("AZ2: utilise le teensy_loader_cli local recompile:", out_bin)
    except Exception as exc:  # pragma: no cover - repli explicite, pas silencieux
        print("AZ2: echec de compilation de teensy_loader_cli local (%s) -- retour au binaire PlatformIO (bug .hex connu sur gros firmwares, voir tools/teensy_loader_cli/README.md)" % exc)


env.AddPreAction("upload", use_local_uploader)
