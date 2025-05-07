# scheduler_dialog.py
import sys, os
from PyQt5 import uic
from PyQt5.QtCore import Qt, pyqtSlot, pyqtSignal, QDateTime # Добавили QDateTime
from PyQt5.QtWidgets import QDialog, QMessageBox, QListWidgetItem # Добавили нужные виджеты

# Путь к UI файлу диалога
BASE_DIR = sys._MEIPASS if getattr(sys, 'frozen', False) \
           else os.path.dirname(os.path.abspath(__file__))
UI_FILE_SCHEDULER = os.path.join(BASE_DIR, "scheduler_dialog.ui") # Убедись, что имя совпадает

class SchedulerDialog(QDialog):
    def __init__(self, client_handler, parent=None):
        print("[DEBUG] SchedulerDialog __init__ started.")
        super().__init__(parent)
        self.client_handler = client_handler # Сохраняем ссылку

        # ---------- Загрузка UI ----------
        if not os.path.exists(UI_FILE_SCHEDULER):
             QMessageBox.critical(self, "Ошибка", f"UI файл не найден: {UI_FILE_SCHEDULER}")
             # Лучше как-то прервать инициализацию, если UI нет
             # Например, вызвать self.close() или выбросить исключение,
             # но для QDialog это может быть сложно обработать извне.
             # Пока просто предупредим и продолжим (что вызовет ошибки ниже)
             print(f"CRITICAL: UI file not found at {UI_FILE_SCHEDULER}")
             # return # Раскомментируй, если хочешь просто закрыть окно при ошибке

        try:
            uic.loadUi(UI_FILE_SCHEDULER, self)
        except Exception as e:
            QMessageBox.critical(self, "Ошибка UI",
                                 f"Не удалось загрузить {UI_FILE_SCHEDULER}:\n{e}")
            # Аналогично, нужно решить, что делать при ошибке загрузки
            print(f"CRITICAL: Failed to load UI: {e}")
            # return # Раскомментируй, если хочешь просто закрыть окно при ошибке

        self.setWindowTitle("Планировщик задач")

        # ---------- Подключение сигналов виджетов ----------
        # Используй objectName, которые ты задал в Qt Designer
        self.addTaskButton.clicked.connect(self.handle_add_task)
        self.refreshButton.clicked.connect(self.request_tasks)
        self.closeButton.clicked.connect(self.accept) # self.accept() закрывает диалог

        # ---------- Подключение сигналов от ClientHandler ----------
        # Эти сигналы нужно будет добавить в ClientHandler на следующем шаге
        if hasattr(self.client_handler, 'scheduler_tasks_received'):
             self.client_handler.scheduler_tasks_received.connect(self.populate_task_list)
             print("[SchedulerDialog] Connected scheduler_tasks_received signal.")
        else:
             print("WARNING: ClientHandler has no signal 'scheduler_tasks_received'")

        if hasattr(self.client_handler, 'scheduler_op_result'):
             self.client_handler.scheduler_op_result.connect(self.show_operation_result)
             print("[SchedulerDialog] Connected scheduler_op_result signal.")
        else:
             print("WARNING: ClientHandler has no signal 'scheduler_op_result'")

        # ---------- Начальное состояние ----------
        # Установим текущую дату и время + 1 час по умолчанию
        self.taskDateTimePicker.setDateTime(QDateTime.currentDateTime().addSecs(3600))
        # Запрашиваем список задач при открытии
        self.request_tasks()

    # --- Методы-слоты для обработки событий GUI ---

    @pyqtSlot()
    def handle_add_task(self):
        """Собирает данные из формы и отправляет команду добавления задачи на сервер."""
        task_name = self.taskNameInput.text().strip()
        description = self.taskDescriptionInput.toPlainText().strip() # Используй .text(), если у тебя QLineEdit
        notify_offset = self.notifyOffsetSpinBox.value()
        trigger_datetime = self.taskDateTimePicker.dateTime() # Получаем QDateTime

        # Проверка на пустые поля
        if not task_name:
            QMessageBox.warning(self, "Ошибка ввода", "Название задачи не может быть пустым.")
            return
        if trigger_datetime <= QDateTime.currentDateTime():
             QMessageBox.warning(self, "Ошибка ввода", "Дата и время срабатывания не могут быть в прошлом.")
             return

        # Форматируем время в строку "ДД.ММ.ГГГГ ЧЧ:ММ", как ожидает сервер
        trigger_time_str = trigger_datetime.toString("dd.MM.yyyy HH:mm")

        # Формируем данные для отправки
        task_details = {
            "task_name": task_name,
            "description": description,
            "trigger_time": trigger_time_str,
            "notify_offset": notify_offset
        }

        # Отправляем команду через ClientHandler (этот метод добавим позже)
        if self.client_handler and hasattr(self.client_handler, 'send_add_task_command'):
            print(f"Sending add task: {task_details}")
            self.client_handler.send_add_task_command(task_details)
            # Очистить поля после успешной отправки? По желанию.
            # self.taskNameInput.clear()
            # self.taskDescriptionInput.clear()
            # self.taskDateTimePicker.setDateTime(QDateTime.currentDateTime().addSecs(3600))
        else:
             QMessageBox.warning(self, "Ошибка", "Не удалось отправить команду добавления задачи (проблема с ClientHandler).")

    # --- Методы для взаимодействия с сервером ---

    @pyqtSlot() # Добавляем декоратор, т.к. подключен к кнопке
    def request_tasks(self):
        """Запрашивает список задач с сервера."""
        if self.client_handler and hasattr(self.client_handler, 'send_list_tasks_command'):
            print("Requesting task list...")
            self.taskListWidget.clear() # Очистим список перед запросом
            self.taskListWidget.addItem("Загрузка списка задач...") # Покажем индикатор
            self.client_handler.send_list_tasks_command() # Этот метод добавим в ClientHandler
        else:
            QMessageBox.warning(self, "Ошибка", "Не удалось запросить задачи (проблема с ClientHandler).")

    # --- Слоты для обработки сигналов от ClientHandler ---

    @pyqtSlot(list)
    def populate_task_list(self, tasks):
        """Обновляет список задач в UI данными, полученными от сервера."""
        print(f"Received tasks: {tasks}")
        self.taskListWidget.clear() # Очищаем список (убираем "Загрузка...")

        if not tasks:
             self.taskListWidget.addItem("Нет активных задач.")
             return

        for task in tasks:
            # Извлекаем данные задачи
            task_id = task.get('id')
            task_name = task.get('name', 'Без имени')
            description = task.get('description', '')
            trigger_iso = task.get('trigger_time_iso')
            # created_iso = task.get('created_at_iso') # Пока не используем

            # Пытаемся преобразовать ISO время в QDateTime
            trigger_dt = QDateTime() # Пустой QDateTime по умолчанию
            if trigger_iso:
                 # Сервер присылает UTC с 'Z'. QDateTime.fromString с Qt.ISODate может его не понять.
                 # Попробуем удалить 'Z' и указать формат явно или использовать ISODateWithMs
                 # Вариант 1: ISODate (может не сработать с 'Z')
                 # trigger_dt = QDateTime.fromString(trigger_iso, Qt.ISODate)
                 # Вариант 2: Убрать Z и указать формат (более надежно)
                 if trigger_iso.endswith('Z'):
                      trigger_iso_no_z = trigger_iso[:-1]
                 else:
                      trigger_iso_no_z = trigger_iso
                 trigger_dt = QDateTime.fromString(trigger_iso_no_z, "yyyy-MM-ddTHH:mm:ss")
                 if trigger_dt.isValid():
                     trigger_dt.setTimeSpec(Qt.UTC) # Указываем, что это время UTC
                 else:
                     print(f"Warning: Could not parse ISO time: {trigger_iso}")

            # Формируем строку для отображения
            display_string = f"[{task_id}] {task_name}"
            if trigger_dt.isValid():
                # Показываем время в локальной таймзоне пользователя
                local_trigger_str = trigger_dt.toLocalTime().toString("dd.MM.yyyy HH:mm")
                display_string += f" (Сработает: {local_trigger_str})"
            if description:
                display_string += f" - {description[:50]}{'...' if len(description)>50 else ''}" # Показываем часть описания

            # Создаем элемент списка
            list_item = QListWidgetItem(display_string)
            # Сохраняем ID задачи в данных элемента для будущего (например, удаления)
            list_item.setData(Qt.UserRole, task_id)
            # Добавляем элемент в виджет списка
            self.taskListWidget.addItem(list_item)

    @pyqtSlot(bool, str)
    def show_operation_result(self, success, message):
        """Показывает результат операции (добавление)."""
        print(f"Operation result: Success={success}, Message='{message}'")
        if success:
            QMessageBox.information(self, "Успех", message)
            self.request_tasks() # Обновляем список после успешной операции добавления
        else:
            QMessageBox.warning(self, "Ошибка операции", message)

# --- Код для тестирования диалога отдельно (если нужно) ---
# if __name__ == '__main__':
#     from PyQt5.QtWidgets import QApplication
#     # Создадим "заглушку" для client_handler для теста
#     class MockClientHandler:
#          scheduler_tasks_received = pyqtSignal(list)
#          scheduler_op_result = pyqtSignal(bool, str)
#          def send_list_tasks_command(self): print("Mock: send_list_tasks_command()")
#          def send_add_task_command(self, details): print(f"Mock: send_add_task_command({details})")
#          # Имитация ответа сервера через 2 секунды
#          def simulate_server_responses(self, dialog):
#               from PyQt5.QtCore import QTimer
#               QTimer.singleShot(1500, lambda: self.scheduler_tasks_received.emit([
#                    {'id': 1, 'name': 'Тест 1', 'description': 'Описание теста 1', 'trigger_time_iso': '2024-08-20T10:00:00Z'},
#                    {'id': 2, 'name': 'Тест 2', 'description': 'Очень длинное описание для второго тестового задания', 'trigger_time_iso': '2024-08-21T15:30:00Z'}
#               ]))
#               # QTimer.singleShot(3000, lambda: self.scheduler_op_result.emit(True, "Задача 'Тест' добавлена (ID: 3)"))
#               # QTimer.singleShot(4000, lambda: self.scheduler_op_result.emit(False, "Ошибка добавления задачи"))


#     app = QApplication(sys.argv)
#     mock_handler = MockClientHandler()
#     dialog = SchedulerDialog(mock_handler)
#     # mock_handler.simulate_server_responses(dialog) # Запуск имитации
#     dialog.show()
#     sys.exit(app.exec_())
