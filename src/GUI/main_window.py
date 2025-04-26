import sys
import os

from PyQt5.QtWidgets import (QMainWindow, QApplication, QMessageBox, QStyle,
                             QWidget, QLineEdit, QListWidget, QListWidgetItem,
                             QLabel)
from PyQt5.QtCore import pyqtSlot, QFile, QIODevice, QSize, Qt
from PyQt5.QtGui import QIcon
from PyQt5 import uic

from style import get_stylesheet

UI_FILE = "mainwindow.ui"
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ICON_SEND_PATH = os.path.join(BASE_DIR, "icons", "send_icon.png")

class MainWindow(QMainWindow):
    def __init__(self, parent=None):
        super().__init__(parent)

        # Загрузка UI файла с помощью PyQt5.uic
        uic.loadUi(UI_FILE, self)
        if hasattr(self, 'chatHeaderLabel'):
            self.chatHeaderLabel.setAlignment(Qt.AlignCenter)
        # Устанавливаем echoMode программно
        self.passwordInput.setEchoMode(QLineEdit.Password)

        # --- Настройка окна и начального состояния ---
        self.setWindowTitle("ClsMess")
        self.statusBar = self.statusBar()
        self.statusBar.showMessage("Введите данные для входа.")

        ### ADDED CODE START ###
        # Переменная для хранения выбранного контакта
        self.current_chat_target = None
        ### ADDED CODE END ###


        # --- Подключение сигналов GUI к слотам ---
        self.loginButton.clicked.connect(self.handle_login)
        self.registerButton.clicked.connect(self.handle_register)
        self.sendButton.clicked.connect(self.handle_send_message)
        self.messageInput.returnPressed.connect(self.handle_send_message)

        ### ADDED CODE START ###
        # Сигнал выбора контакта из нового списка
        # Убедись, что в UI файле у QListWidget objectName="contactListWidget"
        if hasattr(self, 'contactListWidget'): # Проверка на случай, если UI еще не обновлен
            self.contactListWidget.currentItemChanged.connect(self.handle_contact_selected)
        else:
            print("ПРЕДУПРЕЖДЕНИЕ: Виджет 'contactListWidget' не найден в UI.")
        ### ADDED CODE END ###


        # --- Настройка кнопки отправки ---
        self.sendButton.setText("->")
        if os.path.exists(ICON_SEND_PATH):
             send_icon = QIcon(ICON_SEND_PATH)
             self.sendButton.setIcon(send_icon)
             self.sendButton.setIconSize(QSize(0, 0))
             self.sendButton.setToolTip("Отправить сообщение (Enter)")
        else:
             print(f"ВНИМАНИЕ: Файл иконки не найден: {ICON_SEND_PATH}")
             std_icon = self.style().standardIcon(QStyle.SP_ArrowRight)
             self.sendButton.setIcon(std_icon)
             self.sendButton.setIconSize(QSize(20, 20))
             self.sendButton.setToolTip("Отправить сообщение (Enter)")


        # --- Начальное состояние интерфейса ---
        self.mainStackedWidget.setCurrentIndex(0) # Показываем страницу логина
        self.sendButton.setEnabled(False) # Кнопка отправки неактивна
        self.messageInput.setEnabled(False) # Поле ввода неактивно
        self.messageInput.setPlaceholderText("Введите сообщение...") # Плейсхолдер
        self.usernameInput.setPlaceholderText("Ваше имя пользователя")
        self.passwordInput.setPlaceholderText("Ваш пароль")

        ### ADDED CODE START ###
        # Начальный текст заголовка чата
        # Убедись, что в UI файле у QLabel objectName="chatHeaderLabel"
        if hasattr(self, 'chatHeaderLabel'):
            self.chatHeaderLabel.setText("Выберите контакт")
        else:
             print("ПРЕДУПРЕЖДЕНИЕ: Виджет 'chatHeaderLabel' не найден в UI.")

        # Заполняем список контактов (заглушка)
        self.populate_contacts() # Вызываем новый метод
        ### ADDED CODE END ###

        self.log_to_statusbar("Интерфейс загружен.")


    ### ADDED CODE START ###
    def populate_contacts(self):
        """Заполняет список контактов тестовыми данными."""
        if not hasattr(self, 'contactListWidget'):
             return # Выходим, если виджета нет

        self.contactListWidget.clear() # Очищаем на всякий случай
        # Пример данных (замени на реальные, когда будешь подключать бэкенд)
        contacts = ["Алиса Воображаева", "Борис Строитель", "Чарли Чаплин", "Диана Принс", "Проект Феникс"]
        for name in contacts:
            item = QListWidgetItem(name)
            # Здесь можно будет добавить больше информации или иконки
            # item.setIcon(QIcon("путь/к/аватару.png"))
            # item.setData(Qt.UserRole, {"id": f"user_{name.lower()}", "status": "online"})
            self.contactListWidget.addItem(item)
        # self.log_to_statusbar(f"Загружено {len(contacts)} контактов (демо).") # Убрал лог, чтобы не мешать

    @pyqtSlot(QListWidgetItem, QListWidgetItem)
    def handle_contact_selected(self, current_item, previous_item):
        """Обрабатывает выбор контакта в списке."""
        if current_item:
            contact_name = current_item.text()
            self.current_chat_target = contact_name # Сохраняем имя текущего собеседника

            # Обновляем заголовок (если он есть)
            if hasattr(self, 'chatHeaderLabel'):
                self.chatHeaderLabel.setText(contact_name)

            # Очищаем чат (если он есть)
            if hasattr(self, 'chatDisplay'):
                self.chatDisplay.clear()
                self.chatDisplay.append(f"<i>--- Чат с {contact_name} ---</i>")

            # Включаем ввод и отправку
            self.messageInput.setEnabled(True)
            self.sendButton.setEnabled(True)
            self.messageInput.setFocus()
            self.log_to_statusbar(f"Выбран чат с {contact_name}")
        else:
            # Если выделение снято
            self.current_chat_target = None
            if hasattr(self, 'chatHeaderLabel'):
                self.chatHeaderLabel.setText("Выберите контакт")
            if hasattr(self, 'chatDisplay'):
                self.chatDisplay.clear()
            self.messageInput.setEnabled(False)
            self.sendButton.setEnabled(False)
            self.log_to_statusbar("Контакт не выбран.")
    ### ADDED CODE END ###


    @pyqtSlot()
    def handle_login(self):
        username = self.usernameInput.text().strip()
        password = self.passwordInput.text() # Пароль не обрезаем

        if not username or not password:
            self.statusBar.showMessage("Ошибка: Имя и пароль обязательны.", 5000)
            self.usernameInput.setStyleSheet("border: 1px solid red;")
            self.passwordInput.setStyleSheet("border: 1px solid red;")
            return
        else:
             self.usernameInput.setStyleSheet("") # Вернуть стиль из QSS
             self.passwordInput.setStyleSheet("")

        self.log_to_statusbar(f"Вход пользователя '{username}'...")

        # --- Симуляция Успешного Входа ---
        QMessageBox.information(self, "Вход выполнен", f"Добро пожаловать, {username}!")
        self.mainStackedWidget.setCurrentIndex(1) # Переключаемся на страницу чата (индекс 1)
        # ВАЖНО: Следующие строки из ТВОЕГО кода теперь не совсем логичны,
        # так как состояние кнопок/полей должно управляться выбором контакта.
        # Но я их не меняю по твоему запросу.
        self.chatDisplay.clear() # Очищаем чат при входе
        self.chatDisplay.append(f"<i>--- Вы вошли как {username} ---</i>") # Приветствие в чате
        self.messageInput.setFocus() # Фокус на ввод сообщения
        self.sendButton.setEnabled(True) # Активируем кнопку отправки
        self.messageInput.setEnabled(True) # И поле ввода
        # --------------------------------

    @pyqtSlot()
    def handle_register(self):
        username = self.usernameInput.text().strip()
        password = self.passwordInput.text()

        if not username or not password:
            self.statusBar.showMessage("Ошибка: Имя и пароль обязательны для регистрации.", 5000)
            self.usernameInput.setStyleSheet("border: 1px solid red;")
            self.passwordInput.setStyleSheet("border: 1px solid red;")
            return
        else:
             self.usernameInput.setStyleSheet("")
             self.passwordInput.setStyleSheet("")

        reply = QMessageBox.question(self, 'Регистрация',
                                     f'Зарегистрировать пользователя "{username}"?',
                                     QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            self.log_to_statusbar(f"Регистрация пользователя '{username}'...")
            # --- Симуляция Успешной Регистрации ---
            QMessageBox.information(self, "Регистрация", f"Пользователь '{username}' зарегистрирован (симуляция).")
            # -------------------------------------
        else:
             self.statusBar.showMessage("Регистрация отменена.", 3000)


    @pyqtSlot()
    def handle_send_message(self):
        message_text = self.messageInput.text().strip()
        if not message_text:
            return # Не отправляем пустые сообщения

        # --- Симуляция Отправки и Отображения ---
        # ВАЖНО: Этот код использует ЗАХАРДКОЖЕННЫЙ получателя из ТВОЕЙ версии кода.
        # Он не использует выбранный контакт self.current_chat_target.
        # Чтобы использовать выбранный контакт, нужно было бы изменить эту часть.
        # ЗАХАРДКОДИМ ДЛЯ ПРИМЕРА:
        recipient = "user2" # !!! Это из ТВОЕГО старого кода, НЕ используется выбранный контакт !!!
        if not recipient:
             # Эта проверка не сработает для захардкоженного значения
             # self.display_server_error("Не указан получатель сообщения!") # display_server_error не определен
             QMessageBox.warning(self, "Ошибка", "Получатель не указан (внутренняя заглушка)")
             return

        # self.client_handler.send_command(...) # Закомментировано из твоего кода
        self.log_to_statusbar(f"Отправка сообщения...")
        self.display_message("Я", message_text) # Отображаем локально
        # -----------------------------------------

        self.messageInput.clear()
        self.messageInput.setFocus()


    def display_message(self, sender, text):
        sender_html = f"<b>{sender}</b>"
        self.chatDisplay.append(f"{sender_html}: {text}")


    def log_to_statusbar(self, message):
        print(f"LOG: {message}")
        self.statusBar.showMessage(message, 4000)

# --- END OF FILE main_window.py ---
