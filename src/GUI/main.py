# main.py
import sys, os
from PyQt5.QtWidgets import QApplication
from style import get_stylesheet
from client_handler import DEFAULT_HOST, DEFAULT_PORT   #  <-- импорт есть
from main_window import MainWindow

# ---------------- path to resources -----------------
BASE_DIR = sys._MEIPASS if getattr(sys, 'frozen', False) else \
           os.path.dirname(os.path.abspath(__file__))
UI_FILE  = os.path.join(BASE_DIR, 'mainwindow.ui')
ICON_DIR = os.path.join(BASE_DIR, 'icons')
# ----------------------------------------------------

def parse_cli():
    import argparse
    p = argparse.ArgumentParser(description="ClsMess GUI")
    p.add_argument("--host", default=DEFAULT_HOST, help="server host")
    p.add_argument("--port", default=DEFAULT_PORT, type=int, help="server port")
    return p.parse_args()

def main():
    args = parse_cli()

    # Hi-DPI
    from PyQt5.QtCore import Qt
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps,  True)

    app = QApplication(sys.argv)
    app.setStyleSheet(get_stylesheet())

    win = MainWindow(host=args.host, port=args.port)
    win.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()