#main_window.py
import sys, os
import json # Импортируем для работы с JSON командами
import sqlite3

from PyQt5.QtWidgets import (QMainWindow, QApplication, QMessageBox, QStyle,
                             QWidget, QLineEdit, QListWidget, QListWidgetItem,
                             QLabel)
# Импортируем QTcpSocket и связанные классы
from PyQt5.QtCore import pyqtSlot, QFile, QIODevice, QSize, Qt, QObject, pyqtSignal, QByteArray
from PyQt5.QtNetwork import QTcpSocket, QAbstractSocket
from PyQt5.QtGui import QIcon
from PyQt5 import uic
from pathlib import Path

# Предполагаем, что стиль загружается из другого файла
try:
    from style import get_stylesheet
except ImportError:
    print("Предупреждение: Файл style.py не найден, стиль не будет применен.")
    def get_stylesheet(): return ""

# Предполагаем, что ClientHandler находится в client_handler.py
try:
    from client_handler import ClientHandler
except ImportError:
    print("КРИТИЧЕСКАЯ ОШИБКА: Файл client_handler.py не найден!")
    # Создаем класс-заглушку, чтобы избежать ошибок NameError дальше
    class ClientHandler(QObject):
        # Определяем пустые сигналы, чтобы connect не падал
        connected = pyqtSignal()
        disconnected = pyqtSignal()
        error_occurred = pyqtSignal(str)
        server_response = pyqtSignal(dict)
        connection_status = pyqtSignal(bool)
        login_result = pyqtSignal(bool, str)
        register_result = pyqtSignal(bool, str)
        message_received = pyqtSignal(str, str, str)
        server_error = pyqtSignal(str)
        def __init__(self, *args, **kwargs): super().__init__()
        def connect_to_server(self): print("Заглушка ClientHandler: connect_to_server")
        def disconnect_from_server(self): print("Заглушка ClientHandler: disconnect_from_server")
        def send_register_command(self, *args): print("Заглушка ClientHandler: send_register_command")
        def send_login_command(self, *args): print("Заглушка ClientHandler: send_login_command")
        def send_message_command(self, *args): print("Заглушка ClientHandler: send_message_command")


# --- Константы ---
if getattr(sys, 'frozen', False):      # PyInstaller-onefile
    BASE_DIR = sys._MEIPASS
else:
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))

UI_FILE        = os.path.join(BASE_DIR, "mainwindow.ui")
ICON_SEND_PATH = os.path.join(BASE_DIR, "icons", "send_icon.png")

# --- Основной класс окна ---
class MainWindow(QMainWindow):
    def __init__(self, host=None, port=None, parent=None):
        super().__init__(parent)

        # сохраните адрес-порта, пришедшие извне:
        from client_handler import DEFAULT_HOST, DEFAULT_PORT
        self.server_host = host or DEFAULT_HOST
        self.server_port = int(port or DEFAULT_PORT)

        # --- Загрузка UI ---
        try:
            uic.loadUi(UI_FILE, self)
        except Exception as e:
            print(f"КРИТИЧЕСКАЯ ОШИБКА: Не удалось загрузить UI файл '{UI_FILE}': {e}")
            # Показываем сообщение об ошибке и выходим, если UI не загружен
            QMessageBox.critical(None, "Ошибка загрузки UI", f"Не удалось загрузить интерфейс из файла '{UI_FILE}'.\nУбедитесь, что файл существует и корректен.\n\nОшибка: {e}")
            sys.exit(1) # Выход из приложения

        # --- Проверка наличия ключевых виджетов (безопаснее) ---
        required_widgets = ['chatHeaderLabel', 'passwordInput', 'loginButton',
                            'registerButton', 'sendButton', 'messageInput',
                            'contactListWidget', 'mainStackedWidget', 'chatDisplay',
                            'usernameInput']
        missing_widgets = [name for name in required_widgets if not hasattr(self, name)]
        if missing_widgets:
            print(f"ПРЕДУПРЕЖДЕНИЕ: Следующие виджеты не найдены в UI файле: {', '.join(missing_widgets)}")
            # Можно показать QMessageBox или предпринять другие действия

        # --- Настройка UI элементов ---
        self.setWindowTitle("ClsMess v2 (Qt Client)")
        self.statusBar = self.statusBar()

        if hasattr(self, 'chatHeaderLabel'):
            self.chatHeaderLabel.setAlignment(Qt.AlignCenter)
        if hasattr(self, 'passwordInput'):
            self.passwordInput.setEchoMode(QLineEdit.Password)

        # --- Переменные состояния ---
        self.current_chat_target = None # Имя пользователя, с которым открыт чат
        self.current_username = None # Имя пользователя этого клиента после успешного входа
        self.client_handler = None     # Экземпляр обработчика сети

        # --- Настройка и запуск сетевого клиента ---
        # TODO: Замените на реальный IP и порт вашего сервера VDS
        self.server_host = "212.67.17.60"
        self.server_port = 8080           # Пример порта
        self.setup_client_handler()

        # --- Подключение сигналов GUI к слотам ---
        if hasattr(self, 'loginButton'):
            self.loginButton.clicked.connect(self.handle_login)
        if hasattr(self, 'registerButton'):
            self.registerButton.clicked.connect(self.handle_register)
        if hasattr(self, 'sendButton'):
            self.sendButton.clicked.connect(self.handle_send_message)
        if hasattr(self, 'messageInput'):
            self.messageInput.returnPressed.connect(self.handle_send_message)
        if hasattr(self, 'contactListWidget'):
            self.contactListWidget.currentItemChanged.connect(self.handle_contact_selected)

        # --- Настройка кнопки отправки (проверка иконки) ---
        if hasattr(self, 'sendButton'):
            self.sendButton.setText("") # Убираем текст, оставляем иконку
            if os.path.exists(ICON_SEND_PATH):
                self.sendButton.setIcon(QIcon(ICON_SEND_PATH))
                self.sendButton.setIconSize(QSize(24, 24)) # Немного увеличим
            else:
                print(f"ВНИМАНИЕ: Файл иконки не найден: {ICON_SEND_PATH}, используется стандартная.")
                std_icon = self.style().standardIcon(QStyle.SP_ArrowRight) # Стандартная иконка
                self.sendButton.setIcon(std_icon)
                self.sendButton.setIconSize(QSize(20, 20))
            self.sendButton.setToolTip("Отправить сообщение (Enter)")

        # --- Начальное состояние интерфейса ---
        self.set_initial_ui_state()

        self.log_to_statusbar("Интерфейс загружен. Попытка подключения...")

    def set_initial_ui_state(self):
        """Устанавливает начальное состояние виджетов."""
        if hasattr(self, 'mainStackedWidget'):
            self.mainStackedWidget.setCurrentIndex(0) # Страница логина
        if hasattr(self, 'sendButton'):
            self.sendButton.setEnabled(False)
        if hasattr(self, 'messageInput'):
            self.messageInput.setEnabled(False)
            self.messageInput.setPlaceholderText("Выберите чат для начала общения")
        if hasattr(self, 'usernameInput'):
            self.usernameInput.setPlaceholderText("Ваше имя пользователя")
        if hasattr(self, 'passwordInput'):
            self.passwordInput.setPlaceholderText("Ваш пароль")
        if hasattr(self, 'chatHeaderLabel'):
            self.chatHeaderLabel.setText("Выберите контакт")
        if hasattr(self, 'chatDisplay'):
             self.chatDisplay.clear()

    def setup_client_handler(self):
        """Создает, настраивает и подключает сетевой клиент."""
        if self.client_handler:
            self.client_handler.disconnect_from_server()
            # Отключаем старые сигналы, чтобы избежать дублирования
            try:
                self.client_handler.connected.disconnect()
                self.client_handler.disconnected.disconnect()
                self.client_handler.error_occurred.disconnect()
                self.client_handler.connection_status.disconnect()
                self.client_handler.login_result.disconnect()
                self.client_handler.register_result.disconnect()
                self.client_handler.message_received.disconnect()
                self.client_handler.server_error.disconnect()
            except TypeError: # Сигналы еще не были подключены
                 pass

        self.client_handler = ClientHandler(self.server_host, self.server_port, self)

        # Подключаем сигналы ClientHandler к слотам MainWindow
        self.client_handler.connected.connect(self.on_server_connected)
        self.client_handler.disconnected.connect(self.on_server_disconnected)
        self.client_handler.error_occurred.connect(self.on_client_handler_error)
        self.client_handler.connection_status.connect(self.update_connection_status_ui)
        self.client_handler.login_result.connect(self.process_login_result)
        self.client_handler.register_result.connect(self.process_register_result)
        self.client_handler.message_received.connect(self.process_message_received)
        self.client_handler.server_error.connect(self.process_server_error_message)
        # TODO: Подключить сигналы для задач, файлов и т.д.

        self.client_handler.connect_to_server() # Запускаем подключение

    def populate_contacts(self):
        """Читает всех юзеров из messenger.db и заполняет QListWidget.
        Сам текущий пользователь (если уже известен) игнорируется."""
        if not hasattr(self, "contactListWidget"):
            return

        if getattr(sys, 'frozen', False):              # запущено из PyInstaller
            _START_DIR = os.path.dirname(sys.executable)
        else:                                          # обычный .py
            _START_DIR = os.path.dirname(os.path.abspath(__file__))

        DB_PATH = os.path.abspath(os.path.join(_START_DIR, '..', '..', 'app', 'messenger.db'))

        try:
            with sqlite3.connect(DB_PATH) as conn:
                conn.row_factory = lambda cur, row: row[0]          # получаем сразу строки-юзеры
                users = conn.execute("SELECT username FROM users "
                                     "ORDER BY username COLLATE NOCASE;").fetchall()
        except Exception as e:
            print(f"[Contacts] БД: {e}")
            users = []

        my_name = self.current_username          # может быть None до логина
        self.contactListWidget.clear()
        for name in users:
            if name != my_name:                  # убираем себя
                self.contactListWidget.addItem(QListWidgetItem(name))

        # до выбора контакта поля ввода/кнопка должны быть недоступны
        if hasattr(self, "messageInput"):
            self.messageInput.setEnabled(False)
        if hasattr(self, "sendButton"):
            self.sendButton.setEnabled(False)

    # --- Слоты для сигналов GUI ---

    @pyqtSlot()
    def handle_login(self):
        """Обработчик нажатия кнопки Login."""
        username = self.usernameInput.text().strip()
        password = self.passwordInput.text()
        if not username or not password:
            QMessageBox.warning(self, "Ошибка Ввода", "Имя пользователя и пароль не могут быть пустыми.")
            return
        if not self.client_handler:
            QMessageBox.critical(self, "Ошибка Клиента", "Обработчик сети не инициализирован.")
            return

        self.log_to_statusbar(f"Попытка входа как '{username}'...")
        self.current_username = username # Сохраняем имя ДО отправки (для login_result)
        self.client_handler.send_login_command(username, password)

    @pyqtSlot()
    def handle_register(self):
        """Обработчик нажатия кнопки Register."""
        username = self.usernameInput.text().strip()
        password = self.passwordInput.text()
        if not username or not password:
            QMessageBox.warning(self, "Ошибка Ввода", "Имя пользователя и пароль не могут быть пустыми.")
            return
        # TODO: Добавить проверку сложности пароля
        if not self.client_handler:
            QMessageBox.critical(self, "Ошибка Клиента", "Обработчик сети не инициализирован.")
            return

        reply = QMessageBox.question(self, 'Подтверждение Регистрации',
                                     f'Зарегистрировать нового пользователя "{username}"?',
                                     QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            self.log_to_statusbar(f"Регистрация пользователя '{username}'...")
            self.client_handler.send_register_command(username, password)
        else:
            self.log_to_statusbar("Регистрация отменена.")

    @pyqtSlot()
    def handle_send_message(self):
        """Обработчик нажатия кнопки Send или Enter в поле ввода."""
        message_text = self.messageInput.text().strip()
        if not message_text: return # Не отправляем пустое

        if not self.current_chat_target:
            QMessageBox.warning(self, "Ошибка Отправки", "Сначала выберите контакт из списка!")
            return
        if not self.current_username:
             QMessageBox.warning(self, "Ошибка Отправки", "Вы не вошли в систему!")
             return
        if not self.client_handler:
            QMessageBox.critical(self, "Ошибка Клиента", "Обработчик сети не инициализирован.")
            return

        self.log_to_statusbar(f"Отправка сообщения для {self.current_chat_target}...")
        self.client_handler.send_message_command(self.current_chat_target, message_text)

        # Оптимистичное отображение (показываем сразу, не ждем ответа сервера)
        self.display_message("Я", message_text) # Используем "Я" для своих сообщений
        self.messageInput.clear()
        self.messageInput.setFocus()

    @pyqtSlot(QListWidgetItem, QListWidgetItem)
    def handle_contact_selected(self, current_item, previous_item):
        """Обрабатывает выбор контакта в списке."""
        if current_item:
            contact_name = current_item.text()
            if contact_name == self.current_username:
                 QMessageBox.information(self, "Чат с собой", "Вы не можете начать чат с самим собой.")
                 # Сбрасываем выделение или выбираем предыдущий?
                 if previous_item:
                     self.contactListWidget.setCurrentItem(previous_item)
                 else:
                     self.contactListWidget.clearSelection()
                 return

            self.current_chat_target = contact_name # Сохраняем имя

            if hasattr(self, 'chatHeaderLabel'):
                self.chatHeaderLabel.setText(contact_name)
            if hasattr(self, 'chatDisplay'):
                self.chatDisplay.clear()
                self.chatDisplay.append(f"<i>--- Начало чата с {contact_name} ---</i>")
                # TODO: Запросить историю сообщений для этого чата у сервера?
            if hasattr(self, 'messageInput'):
                self.messageInput.setEnabled(True)
                self.messageInput.setFocus()
            if hasattr(self, 'sendButton'):
                self.sendButton.setEnabled(True)
            self.log_to_statusbar(f"Выбран чат с {contact_name}")
        else:
            # Снято выделение
            self.current_chat_target = None
            if hasattr(self, 'chatHeaderLabel'): self.chatHeaderLabel.setText("Выберите контакт")
            if hasattr(self, 'chatDisplay'): self.chatDisplay.clear()
            if hasattr(self, 'messageInput'): self.messageInput.setEnabled(False)
            if hasattr(self, 'sendButton'): self.sendButton.setEnabled(False)
            self.log_to_statusbar("Контакт не выбран.")

    # --- Слоты для сигналов ClientHandler ---

    @pyqtSlot()
    def on_server_connected(self):
        self.log_to_statusbar("Успешно подключено к серверу.")
        # После подключения можно автоматически пытаться войти, если есть сохраненные данные,
        # или просто дать пользователю ввести данные.
        self.usernameInput.setStyleSheet("") # Сброс стилей ошибок
        self.passwordInput.setStyleSheet("")

    @pyqtSlot()
    def on_server_disconnected(self):
        self.log_to_statusbar("Соединение с сервером потеряно.")
        QMessageBox.warning(self, "Разрыв соединения", "Связь с сервером потеряна.\nПожалуйста, проверьте соединение и попробуйте войти снова.")
        self.set_initial_ui_state() # Возврат к начальному состоянию
        self.mainStackedWidget.setCurrentIndex(0) # На страницу логина
        self.current_username = None # Сбрасываем залогиненного пользователя
        self.current_chat_target = None
        # TODO: Добавить логику автоматического переподключения, если нужно

    @pyqtSlot(str)
    def on_client_handler_error(self, error_msg):
        # Обрабатываем ошибки сокета или другие ошибки ClientHandler
        self.log_to_statusbar(f"Ошибка сети/клиента: {error_msg}")
        # Показываем ошибку только если она не связана с простым дисконнектом
        if "Connection refused" in error_msg or "Host not found" in error_msg:
             QMessageBox.critical(self, "Ошибка Подключения", f"Не удалось подключиться к серверу:\n{error_msg}\n\nПроверьте адрес сервера и ваше интернет-соединение.")
             self.mainStackedWidget.setCurrentIndex(0) # На страницу логина
        # Можно добавить обработку других конкретных ошибок

    @pyqtSlot(bool)
    def update_connection_status_ui(self, is_connected):
        # Можно использовать для обновления иконки статуса и т.п.
        print(f"UI: Connection status changed: {is_connected}")
        if not is_connected and self.mainStackedWidget.currentIndex() == 1:
             # Если мы были в чате и соединение разорвалось
             self.on_server_disconnected() # Вызываем общую логику дисконнекта

    @pyqtSlot(bool, str)
    def process_login_result(self, success, message):
        """Обрабатывает результат попытки входа."""
        if success:
            QMessageBox.information(self, "Вход выполнен", f"Добро пожаловать, {self.current_username}!")
            self.mainStackedWidget.setCurrentIndex(1) # Переключаемся на чат
            self.log_to_statusbar(f"Вход как {self.current_username} выполнен.")
            self.populate_contacts()
            # Очистка полей ввода и деактивация до выбора контакта
            if hasattr(self, 'messageInput'): self.messageInput.setEnabled(False)
            if hasattr(self, 'sendButton'): self.sendButton.setEnabled(False)
            if hasattr(self, 'chatHeaderLabel'): self.chatHeaderLabel.setText("Выберите контакт")
            if hasattr(self, 'chatDisplay'): self.chatDisplay.clear()

            # TODO: Запросить список контактов у сервера здесь
            # self.client_handler.send_request_contacts()
        else:
            QMessageBox.warning(self, "Ошибка входа", f"Не удалось войти: {message}")
            self.log_to_statusbar(f"Ошибка входа для {self.current_username}: {message}")
            self.current_username = None # Сбрасываем имя при неудачном логине

    @pyqtSlot(bool, str)
    def process_register_result(self, success, message):
        """Обрабатывает результат попытки регистрации."""
        if success:
            QMessageBox.information(self, "Регистрация", f"Регистрация успешна: {message}\nТеперь вы можете войти.")
            self.log_to_statusbar(f"Регистрация успешна: {message}")
        else:
            QMessageBox.warning(self, "Ошибка регистрации", f"Не удалось зарегистрироваться: {message}")
            self.log_to_statusbar(f"Ошибка регистрации: {message}")

    @pyqtSlot(str, str, str)
    def process_message_received(self, sender, recipient, text):
        """Обрабатывает входящее сообщение от сервера."""
        print(f"DEBUG: Message received: from={sender}, to={recipient}, text={text}")
        # recipient - это имя пользователя этого клиента (если сервер его добавил)
        # Убедимся, что сообщение действительно для нас
        if self.current_username and recipient != self.current_username:
            print(f"WARNING: Received message intended for '{recipient}', but I am '{self.current_username}'. Ignoring.")
            return

        # Проверяем, открыт ли чат с отправителем
        if sender == self.current_chat_target:
            self.display_message(sender, text)
        else:
            # Сообщение от другого пользователя (не в активном чате)
            self.log_to_statusbar(f"Новое сообщение от {sender}!")
            # TODO: Реализовать более явное уведомление
            # - Подсветить имя контакта в списке
            # - Показать всплывающее уведомление
            # - Увеличить счетчик непрочитанных у контакта
            print(f"INFO: New message from '{sender}' (not current chat)")

    @pyqtSlot(str)
    def process_server_error_message(self, error_msg):
        """Обрабатывает сообщение об ошибке от сервера."""
        self.log_to_statusbar(f"Ошибка сервера: {error_msg}")
        QMessageBox.warning(self, "Ошибка Сервера", error_msg)

    # --- Вспомогательные методы ---

    def display_message(self, sender, text):
        """Добавляет сообщение в область чата."""
        if not hasattr(self, 'chatDisplay'): return
        # Экранируем HTML символы в тексте сообщения
        # text_escaped = Qt.convertFromPlainText(text) # Это может быть слишком просто
        import html
        text_escaped = html.escape(text)

        # Форматируем отправителя
        sender_html = f"<b>{html.escape(sender)}</b>"
        if sender == "Я": # Или сравнивать с self.current_username
            # Можно добавить другой стиль для своих сообщений
            sender_html = f"<b style='color: blue;'>Я</b>"

        # Добавляем в QTextEdit
        self.chatDisplay.append(f"{sender_html}: {text_escaped}")

    def log_to_statusbar(self, message, timeout=4000):
        """Выводит сообщение в статус бар и в консоль."""
        print(f"STATUS: {message}")
        if hasattr(self, 'statusBar'):
            self.statusBar.showMessage(message, timeout)

    def closeEvent(self, event):
        """Переопределяем событие закрытия окна для корректного завершения."""
        self.log_to_statusbar("Закрытие приложения...")
        if self.client_handler:
            self.client_handler.disconnect_from_server()
        event.accept() # Принимаем событие закрытия

# --- Точка входа в приложение ---
if __name__ == '__main__':
    # Проверка существования UI файла перед запуском
    if not os.path.exists(UI_FILE):
         print(f"КРИТИЧЕСКАЯ ОШИБКА: UI файл '{UI_FILE}' не найден.")
         # Показываем системное сообщение об ошибке, т.к. QApplication еще не создан
         msg_box = QMessageBox(QMessageBox.Critical, "Ошибка Файла", f"Не найден файл интерфейса: {UI_FILE}\nПриложение не может быть запущено.")
         msg_box.exec_()
         sys.exit(1)

    app = QApplication(sys.argv)
    app.setStyleSheet(get_stylesheet()) # Применяем стиль
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
