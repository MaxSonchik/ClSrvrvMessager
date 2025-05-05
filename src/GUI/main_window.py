"""
main_window.py — главное окно десктоп-клиента ClsMess.

• Список контактов формируется по событию users_list от сервера.
• При выборе контакта автоматически запрашивается история диалога.
• Локальная БД больше не читается.
"""

import sys, os, html
from PyQt5 import uic
from PyQt5.QtCore    import Qt, pyqtSlot, QSize
from PyQt5.QtGui     import QIcon
from PyQt5.QtWidgets import (QApplication, QMainWindow, QMessageBox,
                             QListWidgetItem, QStyle, QLineEdit)

from client_handler import ClientHandler, DEFAULT_HOST, DEFAULT_PORT
from style          import get_stylesheet          # если style.py нет — заглушка внутри


# ────────── ресурсы ──────────
BASE_DIR = sys._MEIPASS if getattr(sys, 'frozen', False) \
           else os.path.dirname(os.path.abspath(__file__))
UI_FILE        = os.path.join(BASE_DIR, "mainwindow.ui")
ICON_SEND_PATH = os.path.join(BASE_DIR, "icons", "send_icon.png")
# ─────────────────────────────


class MainWindow(QMainWindow):
    # --------------------------------------------------
    def __init__(self, host=None, port=None, parent=None):
        super().__init__(parent)

        self.server_host = host or DEFAULT_HOST
        self.server_port = int(port or DEFAULT_PORT)

        # ---------- UI ----------
        try:
            uic.loadUi(UI_FILE, self)
        except Exception as e:
            QMessageBox.critical(None, "Ошибка UI",
                                 f"Не удалось загрузить {UI_FILE}:\n{e}")
            sys.exit(1)

        self.setWindowTitle("ClsMess v2 (Qt Client)")
        self.statusBar = self.statusBar()

        self.chatHeaderLabel.setAlignment(Qt.AlignCenter)
        self.passwordInput.setEchoMode(QLineEdit.Password)

        # ---------- кнопки ----------
        self.loginButton.clicked.connect(self.handle_login)
        self.registerButton.clicked.connect(self.handle_register)
        self.sendButton.clicked.connect(self.handle_send_message)
        self.messageInput.returnPressed.connect(self.handle_send_message)
        self.contactListWidget.currentItemChanged.connect(
            self.handle_contact_selected)

        self.sendButton.setText("")
        if os.path.exists(ICON_SEND_PATH):
            self.sendButton.setIcon(QIcon(ICON_SEND_PATH))
            self.sendButton.setIconSize(QSize(24, 24))
        else:
            self.sendButton.setIcon(
                self.style().standardIcon(QStyle.SP_ArrowRight))
            self.sendButton.setIconSize(QSize(20, 20))
        self.sendButton.setToolTip("Отправить сообщение (Enter)")

        # ---------- состояние ----------
        self.current_chat_target = None
        self.current_username    = None
        self.client_handler      = None

        self.set_initial_ui_state()
        self.setup_client_handler()

    # ---------- инициализация ----------
    def set_initial_ui_state(self):
        self.mainStackedWidget.setCurrentIndex(0)
        self.sendButton.setEnabled(False)
        self.messageInput.setEnabled(False)
        self.messageInput.setPlaceholderText("Выберите чат для начала общения")
        self.usernameInput.setPlaceholderText("Имя пользователя")
        self.passwordInput.setPlaceholderText("Пароль")
        self.chatHeaderLabel.setText("Выберите контакт")
        self.chatDisplay.clear()

    def setup_client_handler(self):
        self.client_handler = ClientHandler(self.server_host,
                                            self.server_port, self)

        ch = self.client_handler
        ch.connected.connect(self.on_server_connected)
        ch.disconnected.connect(self.on_server_disconnected)
        ch.error_occurred.connect(self.on_client_handler_error)
        ch.connection_status.connect(self.update_connection_status_ui)
        ch.login_result.connect(self.process_login_result)
        ch.register_result.connect(self.process_register_result)
        ch.message_received.connect(self.process_message_received)
        ch.server_error.connect(self.process_server_error_message)
        ch.users_updated.connect(self.update_contacts_from_server)
        ch.history_received.connect(self.populate_history)

        ch.connect_to_server()                     # без автологина

    # ---------- работа с контактами ----------
    @pyqtSlot(list)
    def update_contacts_from_server(self, users):
        """users – список dict’ов {'id': int, 'name': str}."""
        self.contactListWidget.clear()
        me = self.current_username
        for u in users:
            if u["name"] != me:
                self.contactListWidget.addItem(QListWidgetItem(u["name"]))

        # пока не выбран чат – поле ввода выключено
        self.messageInput.setEnabled(False)
        self.sendButton.setEnabled(False)

    # ---------- GUI события ----------
    @pyqtSlot()
    def handle_login(self):
        u, p = self.usernameInput.text().strip(), self.passwordInput.text()
        if not u or not p:
            QMessageBox.warning(self, "Пустые поля", "Имя и пароль обязательны.")
            return
        self.current_username = u
        self.log_to_statusbar(f"Входим как {u}…")
        self.client_handler.send_login_command(u, p)

    @pyqtSlot()
    def handle_register(self):
        u, p = self.usernameInput.text().strip(), self.passwordInput.text()
        if not u or not p:
            QMessageBox.warning(self, "Пустые поля", "Имя и пароль обязательны.")
            return
        if QMessageBox.question(self, "Регистрация",
                                f"Зарегистрировать «{u}»?",
                                QMessageBox.Yes | QMessageBox.No) == QMessageBox.Yes:
            self.client_handler.send_register_command(u, p)

    @pyqtSlot()
    def handle_send_message(self):
        txt = self.messageInput.text().strip()
        if not txt:
            return
        if not self.current_chat_target:
            QMessageBox.warning(self, "Нет контакта", "Сначала выберите собеседника.")
            return
        self.client_handler.send_message_command(self.current_chat_target, txt)
        self.display_message("Я", txt)
        self.messageInput.clear()

    @pyqtSlot(QListWidgetItem, QListWidgetItem)
    def handle_contact_selected(self, cur, prev):
        """Выбор контакта в списке."""
        if cur:                                                    # выбран новый элемент
            name = cur.text()
            if name == self.current_username:                      # запрет «чата с собой»
                QMessageBox.information(self, "Чат с собой",
                                        "Нельзя открыть чат с самим собой.")
                self.contactListWidget.setCurrentItem(prev)
                return

            # ——— активируем чат ———
            self.current_chat_target = name
            self.chatHeaderLabel.setText(name)

            self.chatDisplay.clear()                               # очистим, наполним историей
            self.client_handler.request_history(name)              # ← запрос истории у сервера

            self.messageInput.setEnabled(True)
            self.sendButton.setEnabled(True)
            self.messageInput.setFocus()

        else:                                                      # снято выделение
            self.current_chat_target = None
            self.chatHeaderLabel.setText("Выберите контакт")
            self.chatDisplay.clear()
            self.messageInput.setEnabled(False)
            self.sendButton.setEnabled(False)

    @pyqtSlot(list)
    def populate_history(self, msgs):
        """Отрисовывает историю чата, полученную от сервера."""
        self.chatDisplay.clear()
        for m in msgs:
            self.display_message(m["from"], m["text"])

    # ---------- ClientHandler слоты ----------
    @pyqtSlot()
    def on_server_connected(self):
        self.log_to_statusbar("Соединено с сервером.")

    @pyqtSlot()
    def on_server_disconnected(self):
        self.log_to_statusbar("Связь потеряна.")
        self.set_initial_ui_state()
        QMessageBox.warning(self, "Дисконнект", "Соединение с сервером прервано.")

    @pyqtSlot(str)
    def on_client_handler_error(self, msg):
        self.log_to_statusbar(f"Сетевой сбой: {msg}")
        QMessageBox.warning(self, "Ошибка сети", msg)

    @pyqtSlot(bool)
    def update_connection_status_ui(self, ok):
        print("Connection:", ok)

    @pyqtSlot(bool, str)
    def process_login_result(self, ok, msg):
        if ok:
            QMessageBox.information(self, "Вход", "Успешно!")
            self.mainStackedWidget.setCurrentIndex(1)
            self.log_to_statusbar("Вход выполнен.")
            # список контактов придёт сигналом users_updated
        else:
            QMessageBox.warning(self, "Ошибка входа", msg)
            self.current_username = None

    @pyqtSlot(bool, str)
    def process_register_result(self, ok, msg):
        if ok:
            QMessageBox.information(self, "Регистрация", "Готово. Войдите.")
        else:
            QMessageBox.warning(self, "Ошибка регистрации", msg)

    @pyqtSlot(str, str, str)
    def process_message_received(self, sender, to, txt):
        if sender == self.current_chat_target:
            self.display_message(sender, txt)
        else:
            self.log_to_statusbar(f"Новое сообщение от {sender}")

    @pyqtSlot(str)
    def process_server_error_message(self, err):
        QMessageBox.warning(self, "Сервер", err)

    # ---------- утилиты ----------
    def display_message(self, sender, txt):
        self.chatDisplay.append(f"<b>{html.escape(sender)}</b>: {html.escape(txt)}")

    def log_to_statusbar(self, msg, timeout=4000):
        print("STATUS:", msg)
        self.statusBar.showMessage(msg, timeout)

    def closeEvent(self, e):
        if self.client_handler:
            self.client_handler.disconnect_from_server()
        e.accept()


# -------------------------- main --------------------------
if __name__ == "__main__":
    if not os.path.exists(UI_FILE):
        QMessageBox.critical(None, "Ошибка", f"UI '{UI_FILE}' не найден.")
        sys.exit(1)

    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps,  True)

    app = QApplication(sys.argv)
    app.setStyleSheet(get_stylesheet())
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())