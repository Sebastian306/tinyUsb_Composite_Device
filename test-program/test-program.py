from ctypes import windll
from os import path
from sys import argv

OCR_NORMAL = 32512     # Standard arrow
OCR_HAND = 32649       # Hand (wskazujący)
OCR_IBEAM = 32513      # I-beam (tekst)

user32 = windll.user32

CURSOR_FILE_NAME = "cursor.cur"
POINTER_FILE_NAME = "pointer.cur"
STATE_FILE_NAME = "state"
FILES_PATH = "./"

STATE_IS_CUSTOM = "1"
STATE_IS_DEFAULT = "0"

def set_state_file(state):
    state_file_path = path.join(FILES_PATH, STATE_FILE_NAME)
    with open(state_file_path, 'w') as file:
        file.write(str(state))

def read_state_file():
    state_file_path = path.join(FILES_PATH, STATE_FILE_NAME)
    if not path.exists(state_file_path):
        set_state_file(STATE_IS_DEFAULT)
        return STATE_IS_DEFAULT  # Domyślny stan, jeśli plik nie istnieje
    with open(state_file_path, 'r') as file:
        return file.read().strip()


def set_system_cursor(cur_file_path, cursor_id):
    if not path.exists(cur_file_path):
        raise FileNotFoundError(f"Plik kursora nie istnieje: {cur_file_path}")

    hcursor = user32.LoadCursorFromFileW(cur_file_path)
    if not hcursor:
        raise RuntimeError(f"Nie można załadować kursora z pliku: {cur_file_path}")
    if not user32.SetSystemCursor(hcursor, cursor_id):
        raise RuntimeError(f"Nie można ustawić kursora systemowego z pliku: {cur_file_path}")

def reset_cursor():
    SPI_SETCURSORS = 0x0057
    user32.SystemParametersInfoW(SPI_SETCURSORS, 0, None, 0)

if __name__ == "__main__":
    FILES_PATH = argv[1] if len(argv) > 1 else FILES_PATH

    # Pliki kursorów
    cursor_path = path.join(FILES_PATH, CURSOR_FILE_NAME)
    pointer_path = path.join(FILES_PATH, POINTER_FILE_NAME)

    if read_state_file() == STATE_IS_DEFAULT:
        set_system_cursor(cursor_path, OCR_NORMAL)
        set_system_cursor(pointer_path, OCR_HAND)
        set_system_cursor(cursor_path, OCR_IBEAM)
        set_state_file(STATE_IS_CUSTOM)
    else:
        reset_cursor()
        set_state_file(STATE_IS_DEFAULT)
