#!/usr/bin/env bash
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