#!/usr/bin/env bash
sudo apt update&& sudo apt upgrade
sudo apt install libboost-all-dev \
nlohmann-json3-dev \
build-essential \
libsqlite3-dev \
libargon2-dev \
python3.11


set -euo pipefail
cd "$(dirname "$0")"      # ← каталог src/GUI

PY=python3

# ── создаём и активируем venv ───────────────────────────────
if [ ! -d .venv ]; then
  "${PY}" -m venv .venv
fi
source .venv/bin/activate
pip install -U pip wheel
pip install -r requirements.txt
# ─────────────────────────────────────────────────────────────

# ── сборка PyInstaller ──────────────────────────────────────
pyinstaller -F main.py \
  --name messenger_gui \
  --add-data "mainwindow.ui:." \
  --add-data "icons/send_icon.png:icons"
# ─────────────────────────────────────────────────────────────
