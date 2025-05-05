#client_handler.py
"""
Qt-обёртка над TCP-протоколом мессенджера.

▪ При отправке автоматически подставляет поле `event`
  (сервер роутит сообщения именно по event).

▪ Ответы сервера с `event` преобразует в привычную для старого GUI
  схему с `type`, чтобы не переписывать остальной интерфейс.

▪ Новое: поддержка единых пользователей на сервере
  – клиент умеет послать `get_users` и получить `users_list`.
"""
import json
from PyQt5.QtCore    import QObject, pyqtSignal, QByteArray, QTimer
from PyQt5.QtNetwork import QTcpSocket, QAbstractSocket

DEFAULT_HOST = "212.67.17.60"
DEFAULT_PORT = 8080


class ClientHandler(QObject):
    # ────────── сигналы наружу ──────────
    connected         = pyqtSignal()
    disconnected      = pyqtSignal()
    error_occurred    = pyqtSignal(str)

    server_response   = pyqtSignal(dict)          # «сырые» JSON-ы

    connection_status = pyqtSignal(bool)
    login_result      = pyqtSignal(bool, str)
    register_result   = pyqtSignal(bool, str)
    users_updated     = pyqtSignal(list)          # ← список пользователей [{id,name},…]
    message_received  = pyqtSignal(str, str, str)
    server_error      = pyqtSignal(str)
    history_received = pyqtSignal(list)        # список dict
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
        self.socket.errorOccurred.connect(self._on_socket_error)

        # данные для авто-регистрации / логина
        self._pending_credentials = None   # (username, password)

    # ===== public helpers ====================================================
    def connect_to_server(self, username: str = None, password: str = None):
        """
        Можно вызвать с login-данными; тогда сразу после connect
        клиент выполнит register→login→get_users.
        """
        if username and password:
            self._pending_credentials = (username, password)

        if self.socket.state() != QAbstractSocket.ConnectedState:
            print(f"[ClientHandler] Connecting to {self.host}:{self.port}")
            self.buffer.clear()
            self.socket.connectToHost(self.host, self.port)

    def disconnect_from_server(self):
        self.socket.abort()

    # ===== outgoing ==========================================================
    _CMD2EVENT = {
        # «GUI-команда»  →  «event, который понимает C++-сервер»
        "register":      "register",
        "login":         "login",
        "send_message":  "message",
        "add_task":      "add_task",
        "list_tasks":    "list_tasks",
        "get_users":     "get_users",       # ← новое
    }

    _CMD2EVENT["get_history"] = "get_history"

    def _send_json(self, data: dict):
        if self.socket.state() != QAbstractSocket.ConnectedState:
            self.error_occurred.emit("Cannot send: not connected")
            return

        cmd = data.get("command")
        if "event" not in data:
            data["event"] = self._CMD2EVENT.get(cmd, cmd)

        try:
            payload = json.dumps(data, ensure_ascii=False) + "\n"
            self.socket.write(payload.encode("utf-8"))
            print("[ClientHandler] →", payload.rstrip())
        except Exception as e:
            self.error_occurred.emit(f"Send failed: {e}")

    # удобные обёртки ----------------------------------------------------------
    def send_register_command(self, user, pwd):
        self._send_json({"command": "register", "username": user, "password": pwd})

    def send_login_command(self, user, pwd):
        self._send_json({"command": "login", "username": user, "password": pwd})

    def send_message_command(self, to, text):
        self._send_json({"command": "send_message", "to": to, "text": text})

    def request_users_list(self):
        self._send_json({"command": "get_users"})

    def request_history(self, peer):
        self._send_json({"command": "get_history", "with": peer})

    # ===== socket callbacks ===================================================
    def _on_connected(self):
        self._online = True
        self.connected.emit()
        self.connection_status.emit(True)

        # если были переданы креды – регистрируемся, потом логинимся
        if self._pending_credentials:
            user, pwd = self._pending_credentials
            # пробуем register; если юзер уже есть – сервер вернёт error,
            # GUI сможет попробовать login отдельно
            self.send_register_command(user, pwd)

    def _on_disconnected(self):
        self._online = False
        self.disconnected.emit()
        self.connection_status.emit(False)

    def _on_socket_error(self, code):
        msg = f"Socket error: {self.socket.errorString()} (code {code})"
        print("[ClientHandler]", msg)
        self.error_occurred.emit(msg)
        if not self._online:
            self.connection_status.emit(False)

    # ===== incoming data ======================================================
    def _on_ready_read(self):
        self.buffer.append(self.socket.readAll())
        while b"\n" in self.buffer:
            nl = self.buffer.indexOf(b"\n")
            raw = bytes(self.buffer[:nl]).decode("utf-8").strip()
            self.buffer = self.buffer.mid(nl + 1)
            if not raw:
                continue
            try:
                self._route_response(json.loads(raw))
            except json.JSONDecodeError as e:
                self.error_occurred.emit(f"Bad JSON: {e} – [{raw}]")

    # ===== routing ============================================================
    _EVENT2TYPE = {
        "register_success":  "register_result",
        "login_success":     "login_result",
        "users_list":        "users_list",
        "message":           "message",
        "task_list":         "task_list_result",
        "task_notification": "task_notification",
        "status":            "server_status",
        "error":             "server_error",
        "history": "history",
    }

    def _route_response(self, r: dict):
        # превращаем server.event → type
        if "type" not in r and "event" in r:
            ev = r["event"]
            if ev in ("register_success", "login_success"):
                r.setdefault("success", True)
                r.setdefault("message", "")
            elif ev == "error":
                r.setdefault("message", r.get("text", "unknown error"))
            r["type"] = self._EVENT2TYPE.get(ev, ev)

        rtype = r.get("type")
        if not rtype:
            self.error_occurred.emit("Server JSON without 'type'")
            return

        self.server_response.emit(r)            # общий сигнал

        # «узкие» сигналы + автологика ---------------------------------------
        if   rtype == "register_result":
            ok = r.get("success", False)
            self.register_result.emit(ok, r.get("message", ""))
            if ok and self._pending_credentials:
                # после успешной регистрации – логинимся
                u, p = self._pending_credentials
                self.send_login_command(u, p)

        elif rtype == "login_result":
            ok = r.get("success", False)
            self.login_result.emit(ok, r.get("message", ""))
            if ok:
                # после логина сразу просим список пользователей
                self.request_users_list()

        elif rtype == "users_list":
            users = r.get("users", [])
            self.users_updated.emit(users)

        elif rtype == "message":
            self.message_received.emit(r.get("from",""),
                                       r.get("to",""),
                                       r.get("text",""))

        elif rtype == "server_error":
            self.server_error.emit(r.get("message", ""))

        elif rtype == "connection_status":
            self.connection_status.emit(r.get("connected", False))
        elif rtype == "history":
            self.history_received.emit(r.get("messages", []))
        # task_list_result / task_notification можно добавить аналогично
