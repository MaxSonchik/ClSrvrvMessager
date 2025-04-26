import sys
# Используем PyQt5
from PyQt5.QtWidgets import QApplication
from main_window import MainWindow
from style import get_stylesheet

if __name__ == "__main__":
    # Включаем поддержку экранов с высоким разрешением (HiDPI)
    try:
        from PyQt5.QtCore import Qt
        QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
        QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)
    except AttributeError:
        print("Атрибуты HiDPI не найдены (старая версия Qt?).")

    app = QApplication(sys.argv)


    app.setStyleSheet(get_stylesheet())
    # ---------------------------------

    window = MainWindow()
    window.show()

    sys.exit(app.exec_())
