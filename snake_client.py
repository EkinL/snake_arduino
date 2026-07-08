import curses
import websocket

ESP32_IP = "192.168.191.236" 

def main(stdscr):
    curses.curs_set(0)
    stdscr.nodelay(True)
    stdscr.addstr(0, 0, "Fleches pour jouer, q pour quitter")

    ws = websocket.create_connection(f"ws://{ESP32_IP}:81/")

    try:
        while True:
            key = stdscr.getch()

            if key == ord('a'):
                break
            elif key == ord('z'):
                ws.send("U")
            elif key == ord('s'):
                ws.send("D")
            elif key == ord('q'):
                ws.send("L")
            elif key == ord('d'):
                ws.send("R")
    finally:
        ws.close()

curses.wrapper(main)
