#!/usr/bin/env bash

set -euo pipefail
# Переходим в каталог скрипта, чтобы относительные пути работали
cd "$(dirname "$0")"

PY=python3 # Используем системный python3

# --- создаём и активируем venv ---
VENV_DIR=".venv" # Имя каталога venv
if [ ! -d "$VENV_DIR" ]; then
  echo "Creating virtual environment in $VENV_DIR..."
  "${PY}" -m venv "$VENV_DIR"
  if [ $? -ne 0 ]; then echo "Failed to create venv"; exit 1; fi
fi

echo "Activating virtual environment..."
source "$VENV_DIR/bin/activate"

# --- Установка зависимостей ---
echo "Installing/updating Python dependencies from requirements.txt..."
# Обновляем pip и устанавливаем зависимости
pip install -U pip wheel
pip install -r requirements.txt
if [ $? -ne 0 ]; then echo "Failed to install requirements"; exit 1; fi

# --- сборка PyInstaller ---
echo "Building executable with PyInstaller..."
# -F: собрать в один файл
# --name: имя исполняемого файла
# --add-data: включить дополнительные файлы (UI, иконки)
#             формат "ИСТОЧНИК:НАЗНАЧЕНИЕ_ВНУТРИ_EXE"
#             "." означает корень внутри исполняемого файла
pyinstaller -F main.py \
  --name messenger_gui \
  --add-data "mainwindow.ui:." \
  --add-data "icons/send_icon.png:icons" \
  --noconfirm # Перезаписывать выходные файлы без запроса
if [ $? -ne 0 ]; then echo "PyInstaller build failed"; exit 1; fi

echo "GUI build finished successfully. Executable is in dist/ folder."
# Деактивация venv не обязательна, т.к. скрипт завершается
# deactivate
