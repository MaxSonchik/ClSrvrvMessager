# main_window.py
"""
main_window.py — главное окно десктоп-клиента ClsMess.

• Список контактов формируется по событию users_list от сервера.
• При выборе контакта автоматически запрашивается история диалога.
• Локальная БД больше не читается.
• Добавлена кнопка и диалог для планировщика задач.
• Добавлена обработка уведомлений от планировщика.
"""

import sys, os, html
from PyQt5 import uic
from PyQt5.QtCore    import Qt, pyqtSlot, QSize
from PyQt5.QtGui     import QIcon
from PyQt5.QtWidgets import (QApplication, QMainWindow, QMessageBox,
                             QListWidgetItem, QStyle, QLineEdit)

# Импортируем наш обновленный ClientHandler
from client_handler import ClientHandler, DEFAULT_HOST, DEFAULT_PORT
# Импортируем диалог планировщика
from scheduler_dialog import SchedulerDialog
# Стиль (если есть)
from style import get_stylesheet

# ────────── ресурсы ──────────
BASE_DIR = sys._MEIPASS if getattr(sys, 'frozen', False) \
           else os.path.dirname(os.path.abspath(__file__))
UI_FILE        = os.path.join(BASE_DIR, "mainwindow.ui") # Главное окно
ICON_SEND_PATH = os.path.join(BASE_DIR, "icons", "send_icon.png")
# Проверим наличие UI для планировщика, чтобы не падать позже
UI_FILE_SCHEDULER = os.path.join(BASE_DIR, "scheduler_dialog.ui")
# ─────────────────────────────


class MainWindow(QMainWindow):
    # --------------------------------------------------
    def __init__(self, host=None, port=None, parent=None):
        super().__init__(parent)

        self.server_host = host or DEFAULT_HOST
        self.server_port = int(port or DEFAULT_PORT)

        # ---------- UI ----------
        # Проверим наличие основного UI файла перед загрузкой
        if not os.path.exists(UI_FILE):
             QMessageBox.critical(None, "Ошибка UI", f"Файл интерфейса не найден: {UI_FILE}")
             sys.exit(1)
        # Проверим наличие UI файла планировщика (не критично для старта, но нужно для кнопки)
        if not os.path.exists(UI_FILE_SCHEDULER):
             print(f"WARNING: UI file for scheduler not found: {UI_FILE_SCHEDULER}. Scheduler button will not work.")
             # Можно показать QMessageBox.warning здесь, если планировщик - важная часть

        try:
            uic.loadUi(UI_FILE, self)
        except Exception as e:
            QMessageBox.critical(None, "Ошибка UI",
                                 f"Не удалось загрузить {UI_FILE}:\n{e}")
            sys.exit(1)

        self.setWindowTitle("ClsMess")
        self.statusBar = self.statusBar()

        self.chatHeaderLabel.setAlignment(Qt.AlignCenter)
        self.passwordInput.setEchoMode(QLineEdit.Password)

        # ---------- Поиск виджетов и кнопок ----------
        # (Предполагаем, что в mainwindow.ui кнопка имеет objectName='schedulerButton')
        # Если имя другое, замените 'schedulerButton' на правильное.
        # Добавим проверку наличия кнопки, если UI мог быть старым
        self.schedulerButton = getattr(self, 'schedulerButton', None)
        if not self.schedulerButton:
            print("WARNING: QPushButton with objectName='schedulerButton' not found in mainwindow.ui. Scheduler functionality disabled.")
            # Можно создать кнопку программно, если очень нужно, но лучше добавить в .ui

        # --- Стандартные кнопки ---
        self.loginButton.clicked.connect(self.handle_login)
        self.registerButton.clicked.connect(self.handle_register)
        self.sendButton.clicked.connect(self.handle_send_message)
        self.messageInput.returnPressed.connect(self.handle_send_message)
        self.contactListWidget.currentItemChanged.connect(self.handle_contact_selected)

        # --- Кнопка планировщика (если найдена) ---
        if self.schedulerButton:
            self.schedulerButton.clicked.connect(self.open_scheduler_dialog)
            # Можно добавить иконку для планировщика
            # self.schedulerButton.setIcon(self.style().standardIcon(QStyle.SP_FileDialogDetailedView)) # Пример иконки

        # Настройка кнопки отправки
        self.sendButton.setText("->")
        if os.path.exists(ICON_SEND_PATH):
            self.sendButton.setIcon(QIcon(ICON_SEND_PATH))
            self.sendButton.setIconSize(QSize(0, 0)) # Размер подгонится? Или задать явно.
        else:
            self.sendButton.setIcon(self.style().standardIcon(QStyle.SP_ArrowRight))
            self.sendButton.setIconSize(QSize(20, 20))
        self.sendButton.setToolTip("Отправить сообщение (Enter)")

        # ---------- состояние ----------
        self.current_chat_target = None
        self.current_username    = None
        self.client_handler      = None
        self.scheduler_dialog_instance = None # Для хранения экземпляра немодального окна

        self.set_initial_ui_state()
        self.setup_client_handler()

    # ---------- инициализация ----------
    def set_initial_ui_state(self):
        """Устанавливает начальное состояние интерфейса (экран входа)."""
        self.mainStackedWidget.setCurrentIndex(0) # Показываем страницу входа
        # Деактивируем элементы чата
        self.sendButton.setEnabled(False)
        self.messageInput.setEnabled(False)
        self.messageInput.setPlaceholderText("Выберите чат для начала общения")
        self.chatHeaderLabel.setText("Выберите контакт")
        self.chatDisplay.clear()
        self.contactListWidget.clear()
        # Деактивируем кнопку планировщика
        if self.schedulerButton:
            self.schedulerButton.setEnabled(False)
        # Поля входа
        self.usernameInput.setPlaceholderText("Имя пользователя")
        self.passwordInput.setPlaceholderText("Пароль")
        # Сбрасываем состояние
        self.current_chat_target = None
        self.current_username = None
        # Закрываем диалог планировщика, если он был открыт
        if self.scheduler_dialog_instance and self.scheduler_dialog_instance.isVisible():
             self.scheduler_dialog_instance.close()
             self.scheduler_dialog_instance = None


    def setup_client_handler(self):
        """Настраивает обработчик клиента и подключает его сигналы."""
        # Создаем экземпляр нашего обновленного ClientHandler
        self.client_handler = ClientHandler(self.server_host,
                                            self.server_port, self)

        ch = self.client_handler
        # --- Стандартные сигналы ---
        ch.connected.connect(self.on_server_connected)
        ch.disconnected.connect(self.on_server_disconnected)
        ch.error_occurred.connect(self.on_client_handler_error)
        ch.connection_status.connect(self.update_connection_status_ui) # Используется ли этот слот?
        ch.login_result.connect(self.process_login_result)
        ch.register_result.connect(self.process_register_result)
        ch.message_received.connect(self.process_message_received)
        ch.server_error.connect(self.process_server_error_message)
        ch.users_updated.connect(self.update_contacts_from_server)
        ch.history_received.connect(self.populate_history)

        # --- ПОДКЛЮЧЕНИЕ СИГНАЛА УВЕДОМЛЕНИЯ ПЛАНИРОВЩИКА ---
        ch.task_notification_received.connect(self.handle_task_notification)
        # -----------------------------------------------------

        # Пытаемся подключиться к серверу (без автологина здесь, он в connect_to_server)
        ch.connect_to_server()

    # ---------- работа с контактами ----------
    @pyqtSlot(list)
    def update_contacts_from_server(self, users):
        """Обновляет список контактов данными от сервера."""
        # users – список dict’ов {'id': int, 'name': str}.
        self.contactListWidget.clear()
        current_selection = self.current_chat_target # Запомним текущий выбор
        me = self.current_username
        added_items = []
        item_to_select = None

        for u in users:
            if u["name"] != me:
                item = QListWidgetItem(u["name"])
                self.contactListWidget.addItem(item)
                added_items.append(item)
                if u["name"] == current_selection:
                    item_to_select = item # Нашли ранее выбранный контакт

        # Если после обновления списка ранее выбранный контакт остался, выберем его снова
        if item_to_select:
             self.contactListWidget.setCurrentItem(item_to_select)
        # Если ранее не был выбран контакт, или он исчез, сбрасываем состояние чата
        elif not self.current_chat_target and added_items:
             # Можно выбрать первый контакт автоматически или оставить пустым
             # self.contactListWidget.setCurrentRow(0) # Выбрать первый
             self.messageInput.setEnabled(False)
             self.sendButton.setEnabled(False)
             self.chatHeaderLabel.setText("Выберите контакт")
             self.messageInput.setPlaceholderText("Выберите чат для начала общения")
        elif not added_items: # Список контактов пуст
             self.messageInput.setEnabled(False)
             self.sendButton.setEnabled(False)
             self.chatHeaderLabel.setText("Нет доступных контактов")
             self.messageInput.setPlaceholderText("Нет доступных контактов")


    # ---------- GUI события ----------
    @pyqtSlot()
    def handle_login(self):
        """Обрабатывает нажатие кнопки Login."""
        u = self.usernameInput.text().strip()
        p = self.passwordInput.text() # Пароль не тримим
        if not u or not p:
            QMessageBox.warning(self, "Пустые поля", "Имя пользователя и пароль не могут быть пустыми.")
            return
        # Сохраняем имя для последующего использования
        # self.current_username = u # Лучше установить после успешного логина
        self.log_to_statusbar(f"Попытка входа как {u}...")
        # Отправляем команду на сервер
        self.client_handler.send_login_command(u, p)

    @pyqtSlot()
    def handle_register(self):
        """Обрабатывает нажатие кнопки Register."""
        u = self.usernameInput.text().strip()
        p = self.passwordInput.text()
        if not u or not p:
            QMessageBox.warning(self, "Пустые поля", "Имя пользователя и пароль не могут быть пустыми.")
            return

        # Добавим простое подтверждение
        reply = QMessageBox.question(self, "Регистрация нового пользователя",
                                     f"Вы уверены, что хотите зарегистрировать пользователя '{u}'?",
                                     QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            self.log_to_statusbar(f"Попытка регистрации '{u}'...")
            self.client_handler.send_register_command(u, p)

    @pyqtSlot()
    def handle_send_message(self):
        """Отправляет сообщение в текущий выбранный чат."""
        message_text = self.messageInput.text().strip()
        if not message_text:
            return # Не отправляем пустые сообщения

        if not self.current_chat_target:
            QMessageBox.warning(self, "Нет активного чата", "Пожалуйста, выберите контакт для отправки сообщения.")
            return

        # Отправляем команду через ClientHandler
        self.client_handler.send_message_command(self.current_chat_target, message_text)
        # Отображаем отправленное сообщение у себя (сразу, не дожидаясь ответа)
        self.display_message("Я", message_text) # Используем "Я" или self.current_username
        self.messageInput.clear() # Очищаем поле ввода
        self.messageInput.setFocus() # Возвращаем фокус в поле ввода

    @pyqtSlot(QListWidgetItem, QListWidgetItem)
    def handle_contact_selected(self, current_item, previous_item):
        """Обрабатывает выбор контакта в списке."""
        if current_item: # Выбран новый элемент
            contact_name = current_item.text()

            # --- Активируем чат ---
            self.current_chat_target = contact_name
            self.chatHeaderLabel.setText(f"Чат с: {contact_name}") # Обновляем заголовок

            self.chatDisplay.clear() # Очищаем поле чата перед загрузкой истории
            self.chatDisplay.append(f"<i>Загрузка истории с {contact_name}...</i>") # Индикатор
            self.client_handler.request_history(contact_name) # Запрашиваем историю у сервера

            # Активируем поле ввода и кнопку отправки
            self.messageInput.setEnabled(True)
            self.sendButton.setEnabled(True)
            self.messageInput.setPlaceholderText("Введите сообщение...")
            self.messageInput.setFocus() # Устанавливаем фокус на поле ввода

        else: # Снято выделение (например, список очищен)
            self.current_chat_target = None
            self.chatHeaderLabel.setText("Выберите контакт")
            self.chatDisplay.clear()
            self.messageInput.setEnabled(False)
            self.sendButton.setEnabled(False)
            self.messageInput.setPlaceholderText("Выберите чат для начала общения")

    @pyqtSlot(list)
    def populate_history(self, messages: list):
        """Отрисовывает историю чата, полученную от сервера."""
        # Убедимся, что история пришла для текущего чата (на всякий случай)
        # Это можно проверить, если сервер присылает 'with' в ответе history
        # if messages and messages[0].get('with') != self.current_chat_target: return
        if not self.current_chat_target: return # Если чат не выбран, не показываем

        self.chatDisplay.clear() # Очищаем индикатор загрузки
        if not messages:
            self.chatDisplay.append(f"<i>История с {self.current_chat_target} пуста.</i>")
            return

        for msg in messages:
            # Используем html.escape для безопасности
            sender = html.escape(msg.get("from", "Неизвестно"))
            text = html.escape(msg.get("text", ""))
            # Можно добавить время, если сервер его присылает
            # timestamp = msg.get("timestamp", "")
            # display_time = QDateTime.fromString(timestamp, Qt.ISODate).toLocalTime().toString("dd.MM HH:mm")
            self.display_message(sender, text) # Используем существующий метод

    # ---------- ClientHandler слоты ----------
    @pyqtSlot()
    def on_server_connected(self):
        """Вызывается при успешном подключении к серверу."""
        self.log_to_statusbar("Успешно подключено к серверу.")
        # Логин/регистрация инициируются из connect_to_server или кнопок GUI

    @pyqtSlot()
    def on_server_disconnected(self):
        """Вызывается при потере соединения с сервером."""
        self.log_to_statusbar("Соединение с сервером потеряно.")
        QMessageBox.warning(self, "Потеряно соединение",
                            "Связь с сервером прервана. Пожалуйста, перезапустите приложение или проверьте сеть.")
        # Возвращаем интерфейс в начальное состояние (экран входа)
        self.set_initial_ui_state()

    @pyqtSlot(str)
    def on_client_handler_error(self, error_message):
        """Обрабатывает общие ошибки сети или обработки данных от ClientHandler."""
        self.log_to_statusbar(f"Сетевая ошибка: {error_message}", timeout=10000) # Показываем дольше
        # Показываем ошибку пользователю, но не всегда критично
        # QMessageBox.warning(self, "Ошибка сети", error_message)
        print(f"[MainWindow] ClientHandler Error: {error_message}")


    @pyqtSlot(bool)
    def update_connection_status_ui(self, is_connected):
        """Обновляет UI в зависимости от статуса подключения (если нужно)."""
        # Этот слот может быть не нужен, если есть on_connected/on_disconnected
        print(f"[MainWindow] Connection Status Updated: {is_connected}")
        if not is_connected and self.mainStackedWidget.currentIndex() == 1:
             # Если мы были в чате и соединение пропало (но _on_disconnected еще не сработал?)
             self.log_to_statusbar("Соединение потеряно...", 5000)
             # Можно вызвать set_initial_ui_state() здесь, но лучше дождаться on_server_disconnected

    @pyqtSlot(bool, str)
    def process_login_result(self, success, message):
        """Обрабатывает результат попытки входа."""
        if success:
            # Успешный вход! Запоминаем имя пользователя
            # current_username устанавливается здесь, а не при нажатии кнопки
            self.current_username = self.usernameInput.text().strip()

            QMessageBox.information(self, "Вход выполнен", f"Добро пожаловать, {self.current_username}!")
            # Переключаемся на страницу чата
            self.mainStackedWidget.setCurrentIndex(1)
            self.log_to_statusbar(f"Вход выполнен как {self.current_username}.")
            # Активируем кнопку планировщика (если она есть)
            if self.schedulerButton:
                self.schedulerButton.setEnabled(True)
                # --- Отладочный PRINT ---
                print(f"[DEBUG] Scheduler button enabled state after login: {self.schedulerButton.isEnabled()}")
            else:
                # --- Отладочный PRINT ---
                print("[DEBUG] Scheduler button not found during login result processing.")

            # Запрос списка пользователей инициируется из ClientHandler._route_response
            # Очищаем поля ввода пароля
            self.passwordInput.clear()
        else:
            # Ошибка входа
            QMessageBox.warning(self, "Ошибка входа", message)
            self.log_to_statusbar("Ошибка входа.", 5000)
            # Оставляем пользователя на экране входа

    @pyqtSlot(bool, str)
    def process_register_result(self, success, message):
        """Обрабатывает результат попытки регистрации."""
        if success:
            QMessageBox.information(self, "Регистрация успешна",
                                    "Регистрация прошла успешно. Теперь вы можете войти.")
            self.log_to_statusbar("Регистрация успешна.")
            # Очищаем поля, чтобы пользователь ввел их для логина
            # self.usernameInput.clear() # Оставить имя?
            self.passwordInput.clear()
            self.usernameInput.setFocus() # Фокус на имя для входа
        else:
            QMessageBox.warning(self, "Ошибка регистрации", message)
            self.log_to_statusbar("Ошибка регистрации.", 5000)

    @pyqtSlot(str, str, str)
    def process_message_received(self, sender, to_user, text):
        """Обрабатывает входящее сообщение от другого пользователя."""
        # Отображаем сообщение, только если оно пришло от текущего собеседника
        if sender == self.current_chat_target:
            self.display_message(sender, text)
        else:
            # Если сообщение от другого пользователя - показываем уведомление в статусе
            self.log_to_statusbar(f"Новое сообщение от {sender}", 6000)
            # TODO: Можно добавить индикатор непрочитанных сообщений рядом с именем в списке контактов

    @pyqtSlot(str)
    def process_server_error_message(self, error_message):
        """Обрабатывает сообщение об ошибке, пришедшее от сервера (event="error")."""
        QMessageBox.warning(self, "Ошибка сервера", error_message)
        self.log_to_statusbar(f"Ошибка сервера: {error_message}", 8000)

    # ---------- Функционал планировщика ----------
    @pyqtSlot()
    def open_scheduler_dialog(self):
        """Открывает диалоговое окно планировщика задач."""
        # --- Отладочный PRINT ---
        print("[DEBUG] open_scheduler_dialog called!") # <-- Отступ 1 уровня (4 пробела)

        # Проверяем, есть ли соединение
        if not self.client_handler or not self.client_handler.is_connected():
            # --- Отладочный PRINT ---
            print(f"[DEBUG] Scheduler aborted: client_handler={self.client_handler}, is_connected={self.client_handler.is_connected() if self.client_handler else 'N/A'}") # <-- Отступ 2 уровня (8 пробелов)
            # ---------------------------
            QMessageBox.warning(self, "Нет подключения",
                                "Планировщик недоступен без подключения к серверу.") # <-- Отступ 2 уровня (8 пробелов)
            return # <-- Отступ 2 уровня (8 пробелов)

        # Проверяем, найден ли UI файл планировщика
        if not os.path.exists(UI_FILE_SCHEDULER):
            # --- Отладочный PRINT ---
            print(f"[DEBUG] Scheduler aborted: UI file not found at {UI_FILE_SCHEDULER}") # <-- Отступ 2 уровня (8 пробелов)
            # ---------------------------
            # VVV ИСПРАВЛЕННЫЙ ОТСТУП VVV
            QMessageBox.critical(self, "Ошибка", f"UI файл планировщика не найден:\n{UI_FILE_SCHEDULER}") # <-- Отступ 2 уровня (8 пробелов)
            return # <-- Отступ 2 уровня (8 пробелов)
            # ^^^ ИСПРАВЛЕННЫЙ ОТСТУП ^^^

        # --- Отладочный PRINT ---
        print("[DEBUG] Checks passed, attempting to open scheduler dialog...") # <-- Отступ 1 уровня (4 пробела)
        # ---------------------------

        # --- Вариант 2: Немодальный диалог (не блокирует) ---
        # Проверяем, не открыт ли уже диалог
        if self.scheduler_dialog_instance is None or not self.scheduler_dialog_instance.isVisible():
            print("[MainWindow] Opening scheduler dialog...") # <-- Отступ 2 уровня (8 пробелов)
            # Создаем новый экземпляр, передавая client_handler и родителя (self)
            self.scheduler_dialog_instance = SchedulerDialog(self.client_handler, self) # <-- Отступ 2 уровня (8 пробелов)
            # Устанавливаем флаг, чтобы окно удалялось при закрытии (опционально)
            # self.scheduler_dialog_instance.setAttribute(Qt.WA_DeleteOnClose)
            self.scheduler_dialog_instance.show() # Показываем окно # <-- Отступ 2 уровня (8 пробелов)
        else:
            # Если окно уже открыто, просто активируем его
            print("[MainWindow] Scheduler dialog already open, activating.") # <-- Отступ 2 уровня (8 пробелов)
            self.scheduler_dialog_instance.activateWindow() # <-- Отступ 2 уровня (8 пробелов)
            self.scheduler_dialog_instance.raise_() # Поднимаем поверх других окон # <-- Отступ 2 уровня (8 пробелов)

    @pyqtSlot(dict)
    def handle_task_notification(self, notification_data: dict):
        """Обрабатывает сигнал уведомления о задаче от ClientHandler."""
        task_id = notification_data.get('task_id', '?')
        task_name = notification_data.get('task_name', 'Без имени')
        description = notification_data.get('description', 'Нет описания')
        username = notification_data.get('username', '?') # Обычно это имя текущего пользователя

        print(f"[MainWindow] Received task notification: ID={task_id}, Name='{task_name}'")

        # Показываем всплывающее окно с информацией
        QMessageBox.information(self,
                                f"Напоминание: {html.escape(task_name)}",
                                f"<b>Задача #{task_id}</b>\n\n"
                                f"{html.escape(description)}")

        # Дополнительно: если окно планировщика открыто, обновить в нем список
        if self.scheduler_dialog_instance and self.scheduler_dialog_instance.isVisible():
             print("[MainWindow] Scheduler dialog is open, requesting task list update.")
             # Убедимся, что в SchedulerDialog есть метод request_tasks
             if hasattr(self.scheduler_dialog_instance, 'request_tasks'):
                  self.scheduler_dialog_instance.request_tasks()
             else:
                  print("[MainWindow] WARNING: SchedulerDialog has no request_tasks method.")

    # ---------- утилиты ----------
    def display_message(self, sender, text):
        """Добавляет отформатированное сообщение в поле чата."""
        # Используем html.escape для предотвращения инъекций HTML в чат
        safe_sender = html.escape(sender)
        safe_text = html.escape(text).replace("\n", "<br>") # Заменяем переносы строк на <br>

        # Форматируем сообщение (можно добавить время, цвета и т.д.)
        formatted_message = f"<b>{safe_sender}:</b> {safe_text}"
        self.chatDisplay.append(formatted_message)

    def log_to_statusbar(self, message, timeout=4000):
        """Выводит сообщение в статусную строку."""
        print(f"STATUS: {message}") # Дублируем в консоль для отладки
        self.statusBar.showMessage(message, timeout)

    def closeEvent(self, event):
        """Вызывается при закрытии главного окна."""
        # Корректно отключаемся от сервера
        if self.client_handler:
            print("[MainWindow] Closing application, disconnecting client handler...")
            self.client_handler.disconnect_from_server()
        # Закрываем диалог планировщика, если он открыт
        if self.scheduler_dialog_instance:
             self.scheduler_dialog_instance.close()
        event.accept() # Разрешаем закрытие окна

# -------------------------- main --------------------------
if __name__ == "__main__":
    # Проверка наличия основного UI файла вынесена в __init__

    # Настройки для High DPI дисплеев (если нужно)
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)

    app = QApplication(sys.argv)

    # Применяем стиль, если он есть
    style_sheet = get_stylesheet()
    if style_sheet:
        app.setStyleSheet(style_sheet)

    # Создаем и показываем главное окно
    main_win = MainWindow()
    main_win.show()

    sys.exit(app.exec_())
