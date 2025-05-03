import json
from PyQt5.QtCore import QObject, pyqtSignal, QByteArray, QIODevice
from PyQt5.QtNetwork import QTcpSocket, QAbstractSocket

class ClientHandler(QObject):
    # --- Сигналы для MainWindow ---
    connected = pyqtSignal()
    disconnected = pyqtSignal()
    error_occurred = pyqtSignal(str) # Сигнал для общих ошибок сокета/сети
    server_response = pyqtSignal(dict) # Общий сигнал для всех JSON ответов от сервера

    # Сигналы для конкретных событий (альтернатива server_response)
    connection_status = pyqtSignal(bool)
    login_result = pyqtSignal(bool, str)
    register_result = pyqtSignal(bool, str)
    message_received = pyqtSignal(str, str, str) # from, to, text
    server_error = pyqtSignal(str) # Ошибки, о которых сообщил сервер
    # TODO: Добавить сигналы для задач, файлов и т.д.

    def __init__(self, host, port, parent=None):
        super().__init__(parent)
        self.host = host
        self.port = int(port) # QTcpSocket хочет int
        self.socket = QTcpSocket(self)
        self.buffer = QByteArray()
        self._is_connected = False

        # Подключаем сигналы сокета к нашим слотам
        self.socket.connected.connect(self.on_connected)
        self.socket.disconnected.connect(self.on_disconnected)
        self.socket.readyRead.connect(self.on_ready_read)
        # Обработка ошибок сокета
        self.socket.errorOccurred.connect(self.on_socket_error)
        # Альтернативный способ обработки ошибок (старый стиль)
        # self.socket.error.connect(self.on_socket_error_legacy)

    def connect_to_server(self):
        print(f"[ClientHandler] Attempting to connect to {self.host}:{self.port}...")
        if self.socket.state() == QAbstractSocket.ConnectedState:
             print("[ClientHandler] Already connected.")
             return
        self.buffer.clear()
        self.socket.connectToHost(self.host, self.port)
        # Результат будет обработан в on_connected или on_socket_error

    def disconnect_from_server(self):
        print("[ClientHandler] Disconnecting...")
        self.socket.abort() # Или disconnectFromHost()

    def send_command(self, command_data):
        """Отправляет команду (словарь Python) на сервер в виде JSON."""
        if self.socket.state() == QAbstractSocket.ConnectedState:
            try:
                json_string = json.dumps(command_data)
                print(f"[ClientHandler] Sending JSON: {json_string}")
                # Добавляем \n и кодируем в байты
                self.socket.write((json_string + "\n").encode('utf-8'))
                # flush() обычно не требуется для сокетов в Qt
            except Exception as e:
                error_msg = f"Error encoding/sending command: {e}"
                print(f"[ClientHandler] {error_msg}")
                self.error_occurred.emit(error_msg)
        else:
            error_msg = "Cannot send command: Not connected."
            print(f"[ClientHandler] {error_msg}")
            self.error_occurred.emit(error_msg) # Уведомляем об ошибке

    # --- Слоты для сигналов QTcpSocket ---

    def on_connected(self):
        print("[ClientHandler] Connected successfully.")
        self._is_connected = True
        self.connected.emit()
        self.connection_status.emit(True) # Отправляем конкретный сигнал

    def on_disconnected(self):
        print("[ClientHandler] Disconnected from server.")
        self._is_connected = False
        self.disconnected.emit()
        self.connection_status.emit(False)
        # TODO: Попытка переподключения?

    def on_ready_read(self):
        """Читает данные из сокета, буферизует и парсит полные JSON строки."""
        self.buffer.append(self.socket.readAll())

        while b'\n' in self.buffer:
            newline_pos = self.buffer.indexOf(b'\n')
            json_bytes = self.buffer.left(newline_pos)
            self.buffer = self.buffer.mid(newline_pos + 1) # Удаляем обработанные данные + \n

            if json_bytes.isEmpty():
                continue

            try:
                json_str = json_bytes.data().decode('utf-8').strip()
                # print(f"[ClientHandler] Received JSON line: {json_str}") # Отладка
                response = json.loads(json_str)
                self.process_server_response(response) # Обрабатываем ответ
            except json.JSONDecodeError as e:
                 error_msg = f"Failed to parse JSON: {e} - [{json_bytes.data().decode('utf-8', errors='ignore')}]"
                 print(f"[ClientHandler] {error_msg}")
                 self.error_occurred.emit(error_msg)
            except Exception as e:
                 error_msg = f"Error processing received data: {e}"
                 print(f"[ClientHandler] {error_msg}")
                 self.error_occurred.emit(error_msg)

    def on_socket_error(self, socket_error):
        """Обрабатывает ошибки сокета."""
        error_msg = f"Socket Error: {self.socket.errorString()} (Code: {socket_error})"
        print(f"[ClientHandler] {error_msg}")
        # Закрываем сокет при ошибке
        # self.socket.abort() # Может вызвать on_disconnected
        # Уведомляем MainWindow
        self.error_occurred.emit(error_msg)
        # Если была попытка подключения, считаем ее неудавшейся
        if self.socket.state() != QAbstractSocket.ConnectedState and not self._is_connected:
             self.connection_status.emit(False)
             # TODO: Попытка переподключения?

    def process_server_response(self, response):
        """Парсит ответ сервера и эмитирует соответствующие сигналы."""
        response_type = response.get("type") # Используем .get для безопасности
        if not response_type:
            print("[ClientHandler] Received JSON from server without 'type' field.")
            self.error_occurred.emit("Received invalid JSON from server (missing 'type')")
            return

        # Эмитируем общий сигнал
        self.server_response.emit(response)

        # Эмитируем конкретные сигналы
        # (MainWindow может слушать либо общий, либо конкретные)
        if response_type == "register_result":
            self.register_result.emit(response.get("success", False), response.get("message", ""))
        elif response_type == "login_result":
             self.login_result.emit(response.get("success", False), response.get("message", ""))
        elif response_type == "message":
             # Предполагаем, что main_tcp_client добавляет поле 'to'
             self.message_received.emit(response.get("from", ""), response.get("to", ""), response.get("text", ""))
        elif response_type == "server_error":
             self.server_error.emit(response.get("message", "Unknown server error"))
        elif response_type == "connection_status":
             # Этот тип обычно не приходит от сервера, но обрабатываем на всякий случай
             self.connection_status.emit(response.get("connected", False))
        # TODO: Добавить обработку task_list_result, task_notification и т.д.
        else:
            print(f"[ClientHandler] Received unhandled response type: {response_type}")

    # --- Публичные методы для вызова из MainWindow ---
    def send_register_command(self, username, password):
        self.send_command({
            "command": "register",
            "username": username,
            "password": password # TODO: TLS!
        })

    def send_login_command(self, username, password):
        self.send_command({
            "command": "login",
            "username": username,
            "password": password # TODO: TLS!
        })

    def send_message_command(self, recipient, text):
        self.send_command({
            "command": "send_message",
            "to": recipient,
            "text": text
        })
    # TODO: Добавить методы send_add_task, send_list_tasks и т.д.
