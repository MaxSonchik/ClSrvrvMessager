# client_handler.py
"""
Qt-обёртка над TCP-протоколом мессенджера.

- при отправке автоматически добавляет поле `event`
  (сервер роутит сообщения именно по event);
- преобразует ответы сервера с `event` в старую схему
  с `type`, чтобы остальной GUI не менять.
"""
import json
from PyQt5.QtCore    import QObject, pyqtSignal, QByteArray
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
    message_received  = pyqtSignal(str, str, str)
    server_error      = pyqtSignal(str)
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

    # ===== public helpers =====
    def connect_to_server(self):
        if self.socket.state() != QAbstractSocket.ConnectedState:
            print(f"[ClientHandler] Connecting to {self.host}:{self.port}")
            self.buffer.clear()
            self.socket.connectToHost(self.host, self.port)

    def disconnect_from_server(self):
        self.socket.abort()

    # ===== outgoing =====
    _CMD2EVENT = {
        # «GUI-команда»  →  «event, который понимает C++-сервер»
        "register":      "register",
        "login":         "login",
        "send_message":  "message",
        "add_task":      "add_task",
        "list_tasks":    "list_tasks",
    }

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

    # удобные обёртки --------------------------------------------------------
    def send_register_command(self, user, pwd):
        self._send_json({"command": "register", "username": user, "password": pwd})

    def send_login_command(self, user, pwd):
        self._send_json({"command": "login", "username": user, "password": pwd})

    def send_message_command(self, to, text):
        # command == send_message, event будет автоматически подменён на "message"
        self._send_json({"command": "send_message", "to": to, "text": text})

    # ===== socket callbacks =====
    def _on_connected(self):
        self._online = True
        self.connected.emit()
        self.connection_status.emit(True)

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

    # ===== incoming data =====
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

    # ===== routing =====
    _EVENT2TYPE = {
        "register_success":  "register_result",
        "login_success":     "login_result",
        "message":           "message",
        "task_list":         "task_list_result",
        "task_notification": "task_notification",
        "status":            "server_status",
        "error":             "server_error",
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

        # «узкие» сигналы
        if   rtype == "register_result":
            self.register_result.emit(r.get("success", False), r.get("message", ""))
        elif rtype == "login_result":
            self.login_result.emit(r.get("success", False),   r.get("message", ""))
        elif rtype == "message":
            self.message_received.emit(r.get("from",""),
                                       r.get("to",""),
                                       r.get("text",""))
        elif rtype == "server_error":
            self.server_error.emit(r.get("message", ""))
        elif rtype == "connection_status":
            self.connection_status.emit(r.get("connected", False))
        # task_list_result / task_notification можно добавить аналогично