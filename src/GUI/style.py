#style.py

# Темная тема для PyQt5
DARK_BACKGROUND = "#1e1e1e"
DARK_WIDGET_BACKGROUND = "#2d2d2d"
DARK_INPUT_BG = "#3c3c3c"
DARK_TEXT_PRIMARY = "#e0e0e0"
DARK_TEXT_SECONDARY = "#a0a0a0"
DARK_BORDER = "#404040"
DARK_ACCENT = "#007acc"         # Синий акцент
DARK_ACCENT_HOVER = "#3498db"
DARK_ACCENT_PRESSED = "#005c9e" # Чуть темнее синий для нажатия
DARK_SCROLLBAR_HANDLE = "#555555"
DARK_SCROLLBAR_BACKGROUND = DARK_WIDGET_BACKGROUND
DARK_SELECTION_BG = DARK_ACCENT
DARK_PLACEHOLDER_TEXT = "#707070" # Цвет плейсхолдера
DARK_LIST_ITEM_HOVER = "#4a4a4a"  # <--- ДОБАВЛЕНА НОВАЯ КОНСТАНТА

# --- Таблица стилей QSS ---

def get_stylesheet():
    """Возвращает строку QSS для темной темы PyQt5."""
    return f"""
        /* --- Общие стили --- */
        QMainWindow {{
            background-color: {DARK_BACKGROUND};
        }}
        QWidget {{
            background-color: {DARK_WIDGET_BACKGROUND};
            color: {DARK_TEXT_PRIMARY};
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 10pt;
            border: none; /* Убираем границы по умолчанию */
        }}
        QStatusBar {{
            color: {DARK_TEXT_SECONDARY};
            background-color: {DARK_WIDGET_BACKGROUND};
            border-top: 1px solid {DARK_BORDER};
            font-size: 9pt;
        }}

        /* --- Элементы ввода --- */
        QLineEdit {{
            background-color: {DARK_INPUT_BG};
            border: 1px solid {DARK_BORDER};
            border-radius: 15px; /* Более круглые */
            padding: 6px 12px;
            color: {DARK_TEXT_PRIMARY};
            selection-background-color: {DARK_SELECTION_BG};
            selection-color: white; /* Цвет текста при выделении */
        }}
        QLineEdit:focus {{
            border: 1px solid {DARK_ACCENT};
        }}
        QLineEdit[placeholderText] {{
            color: {DARK_PLACEHOLDER_TEXT};
        }}

        QTextEdit {{
            background-color: {DARK_INPUT_BG};
            border: 1px solid {DARK_BORDER};
            border-radius: 5px; /* Менее круглый */
            padding: 6px;
            color: {DARK_TEXT_PRIMARY};
            selection-background-color: {DARK_SELECTION_BG};
            selection-color: white;
        }}
        QTextEdit#chatDisplay {{ /* Специфично для окна чата */
            background-color: {DARK_BACKGROUND}; /* Фон как у окна */
            border: none; /* Уберем рамку у чата */
            border-top: 1px solid {DARK_BORDER}; /* Оставим верхнюю границу */
            border-bottom: 1px solid {DARK_BORDER}; /* И нижнюю */
            border-radius: 0px;
        }}

        /* --- Кнопки --- */
        QPushButton {{
            background-color: {DARK_ACCENT};
            color: white;
            border: none;
            border-radius: 15px;
            padding: 8px 18px;
            min-height: 20px; /* Минимальная высота */
            font-weight: bold;
        }}
        QPushButton:hover {{
            background-color: {DARK_ACCENT_HOVER};
        }}
        QPushButton:pressed {{
            background-color: {DARK_ACCENT_PRESSED};
        }}
        QPushButton:disabled {{
            background-color: {DARK_TEXT_SECONDARY};
            color: {DARK_BACKGROUND};
        }}
        QPushButton#sendButton {{ /* Кнопка отправки - круглая */
             border-radius: 18px; /* Почти круглая */
             padding: 0px;
             min-width: 36px;
             max-width: 36px;
             min-height: 36px;
             max-height: 36px;
        }}

        /* --- Скроллбары --- */
        QScrollBar:vertical {{
            border: none;
            background: {DARK_SCROLLBAR_BACKGROUND};
            width: 10px;
            margin: 0px 0px 0px 0px;
        }}
        QScrollBar::handle:vertical {{
            background: {DARK_SCROLLBAR_HANDLE};
            min-height: 25px;
            border-radius: 5px;
        }}
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
            height: 0px;
            background: none;
        }}
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {{
            background: none;
        }}
        QScrollBar:horizontal {{
            height: 0px; /* Скрываем горизонтальный */
        }}

        /* --- Другие элементы --- */
        QLabel {{
            background-color: transparent; /* Чтобы не перекрывать фон */
            border: none;
        }}
        QStackedWidget {{
            background-color: {DARK_WIDGET_BACKGROUND};
            border: none;
        }}
        QStackedWidget > QWidget {{
             border: none;
             background-color: {DARK_WIDGET_BACKGROUND};
        }}

        /*####################################################################*/
        /*#####                 НОВЫЕ СТИЛИ ВСТАВЛЕНЫ ЗДЕСЬ              #####*/
        /*####################################################################*/

        /* --- Заголовок Чата --- */
        QLabel#chatHeaderLabel {{
            background-color: {DARK_WIDGET_BACKGROUND};
            color: {DARK_TEXT_PRIMARY};
            font-size: 12pt; /* Крупнее */
            font-weight: bold;
            padding: 8px 15px; /* Отступы */
            border-bottom: 1px solid {DARK_BORDER};
            qproperty-alignment: 'AlignCenter'; /* Выравнивание текста по центру */
        }}

        /* --- Список Контактов --- */
        QListWidget#contactListWidget {{
            background-color: {DARK_WIDGET_BACKGROUND};
            border: none;
            border-right: 1px solid {DARK_BORDER}; /* Граница справа */
            outline: 0px; /* Убрать рамку фокуса */
        }}
        QListWidget#contactListWidget::item {{
            color: {DARK_TEXT_PRIMARY};
            padding: 10px 15px; /* Отступы внутри элемента */
            border-bottom: 1px solid {DARK_BORDER}; /* Разделитель */
        }}
        QListWidget#contactListWidget::item:last-child {{
            border-bottom: none; /* Убрать границу у последнего */
        }}
        QListWidget#contactListWidget::item:hover {{
            background-color: {DARK_LIST_ITEM_HOVER};
        }}
        QListWidget#contactListWidget::item:selected {{
            background-color: {DARK_ACCENT};
            color: white; /* Текст в выделенном элементе */
        }}

        /* Контейнер чата справа - без особых стилей, фон наследуется */
        QWidget#chatAreaWidget {{
            border: none;
            background-color: {DARK_WIDGET_BACKGROUND};
        }}
        /*####################################################################*/
        /*#####                 КОНЕЦ НОВЫХ СТИЛЕЙ                       #####*/
        /*####################################################################*/

    """
# --- END OF FILE style.py ---
