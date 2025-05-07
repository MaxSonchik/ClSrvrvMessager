# client_handler.py
"""
Qt-обёртка над TCP-протоколом мессенджера.

▪ При отправке автоматически подставляет поле `event`
  (сервер роутит сообщения именно по event).

▪ Ответы сервера с `event` преобразует в привычную для старого GUI
  схему с `type`, чтобы не переписывать остальной интерфейс.

▪ Новое: поддержка единых пользователей на сервере
  – клиент умеет послать `get_users` и получить `users_list`.

▪ Добавлена поддержка планировщика задач:
  - Отправка команд add_task, list_tasks.
  - Обработка ответов task_list, task_added.
  - Обработка уведомлений task_notification.
"""
import json
from PyQt5.QtCore    import QObject, pyqtSignal, QByteArray, QTimer
from PyQt5.QtNetwork import QTcpSocket, QAbstractSocket

DEFAULT_HOST = "212.67.17.60" # Используй свой хост или DEFAULT_HOST
DEFAULT_PORT = 8080


class ClientHandler(QObject):
    # ────────── сигналы наружу ──────────
    connected         = pyqtSignal()
    disconnected      = pyqtSignal()
    error_occurred    = pyqtSignal(str) # Общие ошибки сокета/обработки

    server_response   = pyqtSignal(dict) # «Сырые» JSON-ы от сервера

    # Сигналы для GUI
    connection_status = pyqtSignal(bool)      # Статус подключения
    login_result      = pyqtSignal(bool, str) # Результат входа (успех, сообщение)
    register_result   = pyqtSignal(bool, str) # Результат регистрации (успех, сообщение)
    users_updated     = pyqtSignal(list)      # Обновленный список пользователей [{id,name},…]
    message_received  = pyqtSignal(str, str, str) # Получено сообщение (от, кому, текст)
    server_error      = pyqtSignal(str)       # Сообщение об ошибке от сервера (event="error")
    history_received  = pyqtSignal(list)      # Получена история сообщений (список dict)

    # --- НОВЫЕ СИГНАЛЫ ДЛЯ ПЛАНИРОВЩИКА ---
    # Сигнал для передачи списка задач (list - список словарей задач)
    scheduler_tasks_received = pyqtSignal(list)
    # Сигнал для результата операций планировщика (добавление) (bool - успех, str - сообщение)
    scheduler_op_result = pyqtSignal(bool, str)
    # Сигнал для уведомления о задаче (dict - полный JSON уведомления от сервера)
    task_notification_received = pyqtSignal(dict)
    # -----------------------------------------
    # ─────────────────────────────────────

    # ------------------------------------------------------------------
    def __init__(self, host: str = DEFAULT_HOST,
                       port: int = DEFAULT_PORT,
                       parent=None):
        super().__init__(parent)
        self.host, self.port = host, int(port)

        self.socket  = QTcpSocket(self)
        self.buffer  = QByteArray()
        self._online = False

        self.socket.connected.connect(self._on_connected)
        self.socket.disconnected.connect(self._on_disconnected)
        self.socket.readyRead.connect(self._on_ready_read)
        # errorOccurred переименован в error в Qt6, но в PyQt5 он errorOccurred
        self.socket.errorOccurred.connect(self._on_socket_error)

        # данные для авто-регистрации / логина
        self._pending_credentials = None   # (username, password)

    # ===== public helpers ====================================================
    def is_connected(self):
        """Возвращает текущий статус подключения."""
        return self._online

    def connect_to_server(self, username: str = None, password: str = None):
        """
        Подключается к серверу.
        Можно вызвать с login-данными; тогда сразу после connect
        клиент попробует register, потом login, потом get_users.
        """
        if username and password:
            self._pending_credentials = (username, password)

        if self.socket.state() != QAbstractSocket.ConnectedState:
            print(f"[ClientHandler] Connecting to {self.host}:{self.port}...")
            self.buffer.clear()
            self.socket.connectToHost(self.host, self.port)
        else:
            print("[ClientHandler] Already connected or connecting.")
            # Если уже подключены и переданы креды, возможно, стоит сразу запустить логин?
            if self._pending_credentials and self._online:
                 print("[ClientHandler] Already connected, attempting login/register sequence.")
                 user, pwd = self._pending_credentials
                 # Сразу логинимся, если уже подключены
                 self.send_login_command(user, pwd)
                 # Или пробуем register, как было? Зависит от логики GUI
                 # self.send_register_command(user, pwd)


    def disconnect_from_server(self):
        print("[ClientHandler] Disconnecting...")
        self._pending_credentials = None # Сбрасываем креды при дисконнекте
        self.socket.abort() # Принудительно закрывает сокет

    # ===== outgoing ==========================================================
    # Сопоставление команды GUI и события (event) для сервера C++
    _CMD2EVENT = {
        "register":      "register",
        "login":         "login",
        "send_message":  "message",
        "get_users":     "get_users",
        "get_history":   "get_history",
        # Команды планировщика
        "add_task":      "add_task",
        "list_tasks":    "list_tasks",
        # "delete_task":   "delete_task", # <-- Раскомментировать, когда добавим удаление
    }

    def _send_json(self, data: dict):
        """Внутренний метод для отправки JSON на сервер."""
        if not self.is_connected():
            msg = "Cannot send data: not connected to the server."
            print(f"[ClientHandler] ERROR: {msg}")
            self.error_occurred.emit(msg) # Сообщаем об ошибке через сигнал
            return

        # Устанавливаем 'event' из 'command', если 'event' еще не задан
        cmd = data.get("command")
        if "event" not in data and cmd:
            event_name = self._CMD2EVENT.get(cmd)
            if event_name:
                data["event"] = event_name
            else:
                # Если команда неизвестна, используем ее как event (может вызвать ошибку на сервере)
                print(f"[ClientHandler] WARNING: Unknown command '{cmd}'. Using it directly as event.")
                data["event"] = cmd

        if "event" not in data:
             msg = f"Cannot send data: missing 'event' or unknown 'command' {cmd}."
             print(f"[ClientHandler] ERROR: {msg}")
             self.error_occurred.emit(msg)
             return

        # Убираем поле 'command', т.к. сервер его не использует (только 'event')
        data.pop("command", None)

        try:
            # Кодируем JSON в UTF-8 и добавляем символ новой строки как разделитель
            payload = json.dumps(data, ensure_ascii=False) + "\n"
            encoded_payload = payload.encode("utf-8")
            bytes_written = self.socket.write(encoded_payload)

            if bytes_written == -1:
                 raise IOError("Socket write error (returned -1)")
            elif bytes_written < len(encoded_payload):
                 print(f"[ClientHandler] WARNING: Partial write ({bytes_written}/{len(encoded_payload)} bytes). Network issues?")
                 # Здесь может потребоваться логика досылки остатка

            print(f"[ClientHandler] → Sent ({bytes_written} bytes): {payload.rstrip()}")
        except Exception as e:
            msg = f"Failed to send JSON data: {e}"
            print(f"[ClientHandler] ERROR: {msg}")
            self.error_occurred.emit(msg)

    # --- Удобные обёртки для отправки команд ---
    def send_register_command(self, user, pwd):
        self._send_json({"command": "register", "username": user, "password": pwd})

    def send_login_command(self, user, pwd):
        self._send_json({"command": "login", "username": user, "password": pwd})

    def send_message_command(self, to, text):
        # Сервер ожидает 'from' в сообщении, но он берет его из сессии.
        # Клиент его не отправляет.
        self._send_json({"command": "send_message", "to": to, "text": text})

    def request_users_list(self):
        self._send_json({"command": "get_users"})

    def request_history(self, peer):
        self._send_json({"command": "get_history", "with": peer})

    # --- Методы для планировщика ---
    def send_add_task_command(self, task_details: dict):
        """Отправляет команду добавления задачи на сервер."""
        # task_details: {"task_name": ..., "description": ..., "trigger_time": ..., "notify_offset": ...}
        payload = {"command": "add_task"}
        payload.update(task_details) # Добавляем все детали из словаря
        self._send_json(payload)

    def send_list_tasks_command(self):
        """Запрашивает список активных задач для текущего пользователя."""
        self._send_json({"command": "list_tasks"})

    # def send_delete_task_command(self, task_id: int): # <-- Раскомментировать, когда добавим удаление
    #     """Отправляет команду удаления задачи."""
    #     self._send_json({"command": "delete_task", "task_id": task_id})

    # ===== socket callbacks ===================================================
    def _on_connected(self):
        print("[ClientHandler] Successfully connected to the server.")
        self._online = True
        self.connected.emit()
        self.connection_status.emit(True)

        # Если были переданы креды при вызове connect_to_server – запускаем авто-вход
        if self._pending_credentials:
            user, pwd = self._pending_credentials
            print(f"[ClientHandler] Credentials pending for '{user}'. Attempting register/login sequence...")
            # Пробуем register; если юзер уже есть – сервер вернёт error,
            # GUI сможет попробовать login отдельно ИЛИ мы автоматически логинимся в _route_response
            self.send_register_command(user, pwd)
            # Важно: сбросим креды после попытки, чтобы не пытаться снова при переподключении
            # self._pending_credentials = None # Или сбросить только после успешного логина? Пока оставим

    def _on_disconnected(self):
        print("[ClientHandler] Disconnected from the server.")
        self._online = False
        # Сбросим креды, чтобы при следующем connect_to_server не пытаться логиниться автоматом, если не переданы новые
        self._pending_credentials = None
        self.disconnected.emit()
        self.connection_status.emit(False)

    def _on_socket_error(self, socket_error: QAbstractSocket.SocketError):
        # Преобразуем код ошибки в читаемое сообщение
        error_string = self.socket.errorString()
        print(f"[ClientHandler] Socket Error: {error_string} (Code: {socket_error})")
        self.error_occurred.emit(f"Network error: {error_string}")
        # Если ошибка произошла до установления соединения или привела к разрыву
        if not self._online or socket_error in [QAbstractSocket.RemoteHostClosedError,
                                                QAbstractSocket.NetworkError,
                                                QAbstractSocket.ConnectionRefusedError]:
            self._online = False # Убедимся, что статус offline
            self.connection_status.emit(False)

    # ===== incoming data ======================================================
    def _on_ready_read(self):
            """Обрабатывает поступление данных в сокет."""
            # Добавляем все доступные данные в буфер
            self.buffer.append(self.socket.readAll()) # self.buffer это QByteArray

            # Обрабатываем все полные сообщения (заканчивающиеся '\n') в буфере
            while True:
                newline_pos = self.buffer.indexOf(b'\n')
                if newline_pos == -1:
                    break # Нет полного сообщения в буфере

                # Извлекаем одно сообщение (до '\n')
                # raw_qbytearray = self.buffer.left(newline_pos) # Получаем QByteArray
                # ИСПРАВЛЕНИЕ: Сразу преобразуем в bytes
                raw_bytes = bytes(self.buffer.left(newline_pos)) # <--- Преобразуем в bytes здесь

                # Удаляем обработанное сообщение и '\n' из буфера
                self.buffer = self.buffer.mid(newline_pos + 1)

                # Пропускаем пустые байты (после преобразования в bytes)
                if not raw_bytes:
                    continue

                try:
                    # Декодируем UTF-8 из объекта bytes
                    raw_string = raw_bytes.decode("utf-8") # <--- Теперь decode работает

                    print(f"[ClientHandler] ← Received ({len(raw_bytes)} bytes): {raw_string}") # Лог входящих
                    json_data = json.loads(raw_string)
                    # Отправляем на маршрутизацию
                    self._route_response(json_data)
                except json.JSONDecodeError as e:
                    # Для сообщения об ошибке тоже используем raw_bytes.decode
                    msg = f"Invalid JSON received: {e} - Raw data: [{raw_bytes.decode('utf-8', errors='ignore')}]"
                    print(f"[ClientHandler] ERROR: {msg}")
                    self.error_occurred.emit(msg)
                except UnicodeDecodeError as e:
                     # Для сообщения об ошибке тоже используем repr(raw_bytes)
                    msg = f"Unicode decode error: {e} - Raw data: [{raw_bytes!r}]"
                    print(f"[ClientHandler] ERROR: {msg}")
                    self.error_occurred.emit(msg)
                except Exception as e:
                    # Ловим другие возможные ошибки при обработке
                    import traceback
                    tb_str = traceback.format_exc()
                    msg = f"Error processing received data: {e}\n{tb_str}" # В лог пишем полный traceback
                    print(f"[ClientHandler] ERROR: {msg}")
                    # В GUI отправляем краткое сообщение
                    self.error_occurred.emit(f"Error processing data: {e}")


    # ===== routing ============================================================
    # Сопоставление события от сервера ('event') внутреннему типу ('type')
    _EVENT2TYPE = {
        # Аутентификация
        "register_success":  "register_result",
        "login_success":     "login_result",
        # Списки
        "users_list":        "users_list",
        "history":           "history_result", # Переименовал для консистентности
        # Сообщения
        "message":           "message",
        # Ошибки и статусы
        "error":             "server_error",
        "status":            "server_status", # Используется ли?
        # Планировщик
        "task_list":         "task_list_result", # Ответ на list_tasks
        "task_added":        "task_added_result", # Ответ на add_task
        "task_notification": "task_notification", # Уведомление от сервера
        # "task_deleted":      "task_deleted_result", # <-- Раскомментировать, когда добавим удаление
    }

    def _route_response(self, response: dict):
        """Маршрутизирует ответ сервера на соответствующие сигналы."""

        event = response.get("event")
        rtype = response.get("type") # Проверяем, может тип уже задан?

        # Если тип не задан, пытаемся определить его из event'а
        if not rtype and event:
            rtype = self._EVENT2TYPE.get(event, event) # Используем event как тип, если нет в словаре
            response["type"] = rtype # Добавляем тип в словарь для единообразия

            # --- Дополнительная обработка для стандартизации ответов ---
            # Устанавливаем 'success' и 'message' для результатов операций
            if event in ("register_success", "login_success", "task_added"): # "task_deleted" сюда же
                response.setdefault("success", True)
                response.setdefault("message", "")
            elif event == "error":
                response.setdefault("success", False)
                # Ищем сообщение об ошибке в полях 'message', 'text' или 'error'
                err_msg = response.get("message", response.get("text", response.get("error", "Unknown server error")))
                response["message"] = err_msg # Сохраняем найденное сообщение в 'message'

        # Если тип все еще не определен, это странный ответ
        if not rtype:
            msg = f"Received response without 'event' or 'type': {response}"
            print(f"[ClientHandler] WARNING: {msg}")
            self.error_occurred.emit(msg)
            return

        # Испускаем "сырой" ответ для возможной общей обработки или логирования
        self.server_response.emit(response)

        # --- Маршрутизация по типу ('rtype') на конкретные сигналы ---
        print(f"[ClientHandler] Routing response type: {rtype}")

        # Аутентификация
        if rtype == "register_result":
            ok = response.get("success", False)
            msg = response.get("message", "Registration failed")
            self.register_result.emit(ok, msg)
            # Если регистрация успешна И были данные для автологина - логинимся
            if ok and self._pending_credentials:
                user, pwd = self._pending_credentials
                print(f"[ClientHandler] Auto-login after successful registration for '{user}'.")
                self.send_login_command(user, pwd)
            # Если регистрация не удалась (юзер есть), а креды были - пробуем логин
            elif not ok and self._pending_credentials:
                 user, pwd = self._pending_credentials
                 # Проверяем типичное сообщение об ошибке
                 if "exist" in msg.lower(): # Уточнить сообщение сервера
                      print(f"[ClientHandler] Registration failed (likely exists), attempting login for '{user}'.")
                      self.send_login_command(user, pwd)

        elif rtype == "login_result":
            ok = response.get("success", False)
            msg = response.get("message", "Login failed")
            self.login_result.emit(ok, msg)
            if ok:
                # После успешного логина сбрасываем pending credentials
                self._pending_credentials = None
                # И запрашиваем список пользователей
                print("[ClientHandler] Login successful. Requesting user list...")
                self.request_users_list()
            else:
                 # Если логин не удался, возможно, стоит сбросить креды?
                 # self._pending_credentials = None
                 pass

        # Списки
        elif rtype == "users_list":
            users = response.get("users", [])
            if isinstance(users, list):
                 self.users_updated.emit(users)
            else:
                 print(f"[ClientHandler] WARNING: Invalid 'users' format in users_list: {users}")

        elif rtype == "history_result": # Используем новое имя типа
            messages = response.get("messages", [])
            if isinstance(messages, list):
                self.history_received.emit(messages)
            else:
                print(f"[ClientHandler] WARNING: Invalid 'messages' format in history_result: {messages}")

        # Сообщения
        elif rtype == "message":
            sender = response.get("from", "")
            to_user = response.get("to", "") # Сервер присылает 'to'? Если нет, убрать
            text = response.get("text", "")
            self.message_received.emit(sender, to_user, text)

        # Ошибки сервера
        elif rtype == "server_error":
            # Сообщение уже извлечено в 'message' при обработке event="error"
            error_msg = response.get("message", "Unknown server error")
            self.server_error.emit(error_msg)
            # Проверяем, не связана ли ошибка с планировщиком, и дублируем в scheduler_op_result?
            # Например, если ошибка пришла в ответ на add_task. Это сложно определить точно.
            # Пока оставляем только server_error. Можно добавить контекст в GUI, если нужно.
            # self.scheduler_op_result.emit(False, f"Server error: {error_msg}") # Дублировать?

        # Статус сервера (если используется)
        elif rtype == "server_status":
             print(f"[ClientHandler] Server status update: {response}")
             # self.connection_status.emit(response.get("connected", False)) # Пример

        # --- Обработка ответов планировщика ---
        elif rtype == "task_list_result":
            tasks = response.get("tasks", [])
            if isinstance(tasks, list):
                 self.scheduler_tasks_received.emit(tasks)
            else:
                 print(f"[ClientHandler] WARNING: Invalid 'tasks' format in task_list_result: {tasks}")
                 self.scheduler_tasks_received.emit([]) # Отправляем пустой список

        elif rtype == "task_added_result":
            # 'success' и 'message' уже установлены при обработке event="task_added"
            ok = response.get("success", False)
            task_id_str = f" (ID: {response.get('task_id', '?')})" if ok else ""
            msg = response.get("message", f"Задача добавлена{task_id_str}" if ok else "Ошибка добавления задачи")
            self.scheduler_op_result.emit(ok, msg)

        elif rtype == "task_notification":
            # Просто пересылаем весь словарь с данными уведомления
            self.task_notification_received.emit(response)

        # elif rtype == "task_deleted_result": # <-- Когда добавим удаление
        #     ok = response.get("success", True) # Обычно удаление без ошибок успешно
        #     msg = response.get("message", "Задача удалена" if ok else "Ошибка удаления задачи")
        #     self.scheduler_op_result.emit(ok, msg) # Используем тот же сигнал для результата

        # Неизвестный тип ответа
        else:
            print(f"[ClientHandler] Received unhandled response type: '{rtype}'. Data: {response}")

# ----- Тестовый запуск (если нужно) -----
if __name__ == '__main__':
    from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QTextEdit, QLineEdit, QPushButton
    import sys

    class TestWindow(QMainWindow):
        def __init__(self):
            super().__init__()
            self.client = ClientHandler() # Указать хост/порт, если не дефолтные

            self.log_edit = QTextEdit()
            self.log_edit.setReadOnly(True)
            self.cmd_input = QLineEdit()
            self.send_button = QPushButton("Send JSON / Command")

            layout = QVBoxLayout()
            layout.addWidget(self.log_edit)
            layout.addWidget(self.cmd_input)
            layout.addWidget(self.send_button)

            central_widget = QWidget()
            central_widget.setLayout(layout)
            self.setCentralWidget(central_widget)

            # Подключаем сигналы клиента к логу
            self.client.connected.connect(lambda: self.log("CONNECTED"))
            self.client.disconnected.connect(lambda: self.log("DISCONNECTED"))
            self.client.error_occurred.connect(lambda msg: self.log(f"ERROR: {msg}"))
            self.client.server_response.connect(lambda data: self.log(f"RAW RESPONSE: {data}"))
            self.client.login_result.connect(lambda ok, msg: self.log(f"LOGIN: {ok}, {msg}"))
            self.client.message_received.connect(lambda f, t, txt: self.log(f"MSG from {f}: {txt}"))
            self.client.users_updated.connect(lambda users: self.log(f"USERS: {users}"))
            self.client.history_received.connect(lambda msgs: self.log(f"HISTORY: {msgs}"))
            self.client.scheduler_tasks_received.connect(lambda tasks: self.log(f"TASKS: {tasks}"))
            self.client.scheduler_op_result.connect(lambda ok, msg: self.log(f"SCHEDULER OP: {ok}, {msg}"))
            self.client.task_notification_received.connect(lambda data: self.log(f"NOTIFICATION: {data}"))

            self.send_button.clicked.connect(self.send_command)
            self.cmd_input.returnPressed.connect(self.send_command)

            self.cmd_input.setPlaceholderText('Enter JSON or command (e.g., connect, login user pass, list_tasks)')

        def log(self, message):
            self.log_edit.append(str(message))

        def send_command(self):
            text = self.cmd_input.text().strip()
            if not text: return

            if text.lower() == 'connect':
                self.client.connect_to_server()
            elif text.lower().startswith('login '):
                parts = text.split(' ', 2)
                if len(parts) == 3:
                    self.client.send_login_command(parts[1], parts[2])
                else: self.log("Usage: login <user> <pass>")
            elif text.lower().startswith('register '):
                 parts = text.split(' ', 2)
                 if len(parts) == 3:
                      self.client.send_register_command(parts[1], parts[2])
                 else: self.log("Usage: register <user> <pass>")
            elif text.lower().startswith('msg '):
                 parts = text.split(' ', 2)
                 if len(parts) == 3:
                      self.client.send_message_command(parts[1], parts[2])
                 else: self.log("Usage: msg <to_user> <text>")
            elif text.lower() == 'list_users':
                 self.client.request_users_list()
            elif text.lower() == 'list_tasks':
                 self.client.send_list_tasks_command()
            elif text.lower().startswith('add_task '):
                 # Пример: add_task MyTask;Описание;01.09.2024 10:00;5
                 parts = text.split(' ', 1)
                 if len(parts) == 2:
                      details_parts = parts[1].split(';', 3)
                      if len(details_parts) == 4:
                           try:
                                task_details = {
                                     "task_name": details_parts[0],
                                     "description": details_parts[1],
                                     "trigger_time": details_parts[2],
                                     "notify_offset": int(details_parts[3])
                                }
                                self.client.send_add_task_command(task_details)
                           except Exception as e: self.log(f"Invalid add_task format or data: {e}")
                      else: self.log("Usage: add_task <name>;<desc>;<dd.mm.yyyy hh:mm>;<offset_min>")
                 else: self.log("Usage: add_task ...")
            elif text.lower() == 'disconnect':
                 self.client.disconnect_from_server()
            elif text.startswith('{') and text.endswith('}'):
                try:
                    json_data = json.loads(text)
                    # Если это просто JSON, предполагаем, что 'event' уже есть
                    self.client._send_json(json_data) # Используем внутренний метод для прямого JSON
                except json.JSONDecodeError as e:
                    self.log(f"Invalid JSON: {e}")
            else:
                self.log(f"Unknown command: {text}")

            self.cmd_input.clear()

        def closeEvent(self, event):
            self.client.disconnect_from_server()
            event.accept()

    app = QApplication(sys.argv)
    win = TestWindow()
    win.show()
    sys.exit(app.exec_())
