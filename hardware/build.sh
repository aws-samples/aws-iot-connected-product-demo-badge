#!/usr/bin/env bash

set -o nounset
set -o pipefail
set -o xtrace
set -o errexit

cd "$(dirname ${BASH_SOURCE})"

main_schematic_file=./demo-badge-2023.kicad_sch
main_board_file=./demo-badge-2023.kicad_pcb
target=generated/Manufacturers/JLCPCB/cpl_jlcpcb.csv

docker run \
    --rm \
    --volume "$PWD":/kicad \
    --volume "$PWD"/.cache:/root/.cache \
    --workdir /kicad \
    ghcr.io/inti-cmnb/kicad7_auto_full:latest \
    kibot \
    --plot-config kibot-default.yaml \
    --schematic ${main_schematic_file} \
    --board-file ${main_board_file}

docker run \
    --rm \
    --volume "$PWD":/kicad \
    --volume "$PWD"/.cache:/root/.cache \
    --workdir /kicad \
    ghcr.io/inti-cmnb/kicad7_auto_full:latest \
    kibot \
    --plot-config kibot-fusionpcb.yaml \
    --schematic ${main_schematic_file} \
    --board-file ${main_board_file} \
    --out-dir generated/single

docker run \
    --rm \
    --volume "$PWD":/kicad \
    --volume "$PWD"/.cache:/root/.cache \
    --workdir /kicad \
    ghcr.io/inti-cmnb/kicad7_auto_full:latest \
    kibot \
    --plot-config kibot-fusionpcb.yaml \
    --schematic ${main_schematic_file} \
    --board-file generated/panel/demo-badge-2023-panel.kicad_pcb \
    --out-dir generated/panel

docker run \
    --rm \
    --volume "$PWD":/kicad \
    --volume "$PWD"/.cache:/root/.cache \
    --workdir /kicad \
    ghcr.io/inti-cmnb/kicad7_auto_full:latest \
    kibot \
    --plot-config kibot-jlcpcb.yaml \
    --schematic ${main_schematic_file} \
    --board-file ${main_board_file} \
    --out-dir generated/single

docker run \
    --rm \
    --volume "$PWD":/kicad \
    --volume "$PWD"/.cache:/root/.cache \
    --workdir /kicad \
    ghcr.io/inti-cmnb/kicad7_auto_full:latest \
    kibot \
    --plot-config kibot-jlcpcb.yaml \
    --schematic ${main_schematic_file} \
    --board-file generated/panel/demo-badge-2023-panel.kicad_pcb \
    --out-dir generated/panel

say Done
