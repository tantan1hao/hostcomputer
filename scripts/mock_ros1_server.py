#!/usr/bin/env python3
import json
import socket
import threading
import time

HOST = "127.0.0.1"
PORT = 9090
DROP_INTERVAL_SEC = 15


def client_worker(conn, addr):
    print(f"[mock] client connected: {addr}")
    conn.settimeout(1.0)
    started = time.time()
    try:
        while True:
            if time.time() - started > DROP_INTERVAL_SEC:
                print("[mock] force drop connection for reconnect test")
                conn.close()
                return

            try:
                data = conn.recv(4096)
            except socket.timeout:
                continue

            if not data:
                return

            for line in data.split(b"\n"):
                if not line.strip():
                    continue
                try:
                    msg = json.loads(line.decode("utf-8", errors="ignore"))
                except Exception:
                    continue

                mtype = msg.get("type", "unknown")
                if mtype == "heartbeat":
                    resp = {"type": "system_status", "mock": True, "ts": int(time.time() * 1000)}
                    conn.sendall((json.dumps(resp, ensure_ascii=False) + "\n").encode("utf-8"))
                elif mtype == "cmd_vel":
                    resp = {"type": "system_status", "ack": "cmd_vel", "ts": int(time.time() * 1000)}
                    conn.sendall((json.dumps(resp, ensure_ascii=False) + "\n").encode("utf-8"))
    finally:
        try:
            conn.close()
        except Exception:
            pass
        print(f"[mock] client disconnected: {addr}")


def main():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(5)
        print(f"[mock] ROS1 mock server listening on {HOST}:{PORT}")
        while True:
            conn, addr = s.accept()
            t = threading.Thread(target=client_worker, args=(conn, addr), daemon=True)
            t.start()


if __name__ == "__main__":
    main()
