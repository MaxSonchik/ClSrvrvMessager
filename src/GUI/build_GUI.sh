#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"        # в папку GUI

python3 -m pip install -r requirements.txt
python3 -m pip install pyinstaller      # локально в venv / user-site

# чистим старый билд
rm -rf build dist messenger_gui.spec

# собираем
pyinstaller messenger_gui.spec --onefile --windowed
echo
echo "Готово: ./dist/messenger_gui  (или messenger_gui.app на macOS)"