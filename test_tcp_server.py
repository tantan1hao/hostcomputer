"""
TCP测试服务端 - 模拟ROS下位机
==============================
功能:
  1. TCP 服务器监听 9090，等待上位机连接
  2. 连接后发送 motor_state / camera_info / IMU / CO2 等测试数据
  3. 接收 operator_input 输入快照，并对关键命令返回 ACK
  4. 可选启动 FFmpeg RTSP 推流（需要 mediamtx + ffmpeg）

用法:
  python test_tcp_server.py                # 基础TCP测试
  python test_tcp_server.py --rtsp         # 同时启动RTSP推流
  python test_tcp_server.py --rtsp -n 3    # 3路摄像头推流
  python test_tcp_server.py -p 9091        # 自定义端口

交互命令 (连接后):
  回车/1  - 发送 motor_state（随机数据）
  2       - 发送 system_status
  3       - 发送 IMU 数据
  4       - 发送 CO2 数据
  5       - 发送 camera_info（需要 --rtsp）
  loop    - 持续发送 motor_state（100ms间隔）
  stop    - 停止持续发送
  q       - 断开连接
"""

import socket
import json
import time
import sys
import threading
import argparse
import random
import shutil
import subprocess
import math


def now_ms():
    return int(time.time() * 1000)


# ============================================
# FFmpeg RTSP 推流
# ============================================

def check_ffmpeg():
    if shutil.which("ffmpeg") is None:
        print("[错误] 未找到 ffmpeg，RTSP 推流不可用")
        return False
    return True


def start_ffmpeg_stream(camera_id, width=1280, height=720, fps=30, rtsp_port=8554):
    """启动一路 FFmpeg 测试推流，带实时时间戳水印"""
    rtsp_url = f"rtsp://127.0.0.1:{rtsp_port}/cam{camera_id}"

    drawtext = (
        "drawtext=fontsize=48:fontcolor=white:box=1:boxcolor=black@0.7:"
        "boxborderw=8:x=(w-text_w)/2:y=h-th-40:"
        f"text='CAM{camera_id} %{{localtime\\:%H\\\\\\:%M\\\\\\:%S}}"
        f".%{{eif\\:mod(t*1000\\,1000)\\:d\\:3}}'"
    )

    cmd = [
        "ffmpeg", "-re",
        "-f", "lavfi",
        "-i", f"testsrc2=size={width}x{height}:rate={fps}",
        "-vf", drawtext,
        "-c:v", "libx264", "-preset", "ultrafast", "-tune", "zerolatency",
        "-g", str(fps), "-b:v", "2000k",
        "-f", "rtsp", "-rtsp_transport", "tcp",
        rtsp_url,
    ]

    print(f"[推流] cam{camera_id} -> {rtsp_url}")
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    return proc, rtsp_url


# ============================================
# JSON 消息构造
# ============================================

def make_motor_state():
    """随机 motor_state"""
    joints = []
    for i in range(6):
        joints.append({
            "position": round(random.uniform(-3.14, 3.14), 3),
            "current": round(random.uniform(0, 1.0), 3),
        })
    return {
        "type": "motor_state",
        "joints": joints,
        "executor_position": round(random.uniform(0, 2), 3),
        "executor_torque": round(random.uniform(0, 5), 3),
        "executor_flags": 1,
        "reserved": 0,
    }


def make_imu_data():
    """随机 IMU 数据"""
    t = time.time()
    return {
        "type": "imu_data",
        "roll": round(15 * math.sin(t * 0.5), 2),
        "pitch": round(10 * math.cos(t * 0.3), 2),
        "yaw": round((t * 10) % 360 - 180, 2),
        "accel_x": round(random.uniform(-0.5, 0.5), 3),
        "accel_y": round(random.uniform(-0.5, 0.5), 3),
        "accel_z": round(9.8 + random.uniform(-0.1, 0.1), 3),
    }


def make_co2_data():
    """随机 CO2 数据"""
    return {
        "type": "co2_data",
        "ppm": round(400 + random.uniform(0, 600), 1),
    }


def make_camera_info(camera_id, rtsp_url, online=True, width=1280, height=720, fps=30):
    """camera_info 消息"""
    return {
        "type": "camera_info",
        "camera_id": camera_id,
        "online": online,
        "codec": "h264",
        "width": width,
        "height": height,
        "fps": fps,
        "bitrate_kbps": 2000,
        "rtsp_url": rtsp_url,
    }


def make_heartbeat():
    return {"type": "heartbeat", "timestamp_ms": now_ms()}


def make_heartbeat_ack(msg):
    return {
        "type": "heartbeat_ack",
        "protocol_version": 1,
        "seq": msg.get("seq", 0),
        "timestamp_ms": now_ms(),
        "server_time_ms": now_ms(),
    }


def make_ack(msg, ok=True, code=0, message="ok"):
    return {
        "type": "ack",
        "protocol_version": 1,
        "ack_type": msg.get("type", "unknown"),
        "seq": msg.get("seq", 0),
        "ok": ok,
        "code": code,
        "message": message,
        "timestamp_ms": now_ms(),
    }


def make_hello():
    return {
        "type": "hello",
        "protocol_version": 1,
        "seq": 0,
        "timestamp_ms": now_ms(),
        "bridge_name": "manual_tcp_test_server",
        "bridge_version": "manual-1.0",
        "robot_id": "manual_test_robot",
    }


def make_capabilities(cameras):
    return {
        "type": "capabilities",
        "protocol_version": 1,
        "seq": 0,
        "timestamp_ms": now_ms(),
        "supports": [
            "operator_input",
            "heartbeat_ack",
            "critical_ack",
            "sync_request",
            "camera_list_request",
        ],
        "max_frame_bytes": 1048576,
        "cameras": [make_camera_info(cam_id, url) for cam_id, url in cameras],
    }


def make_system_snapshot(msg):
    return {
        "type": "system_snapshot",
        "protocol_version": 1,
        "seq": msg.get("seq", 0),
        "timestamp_ms": now_ms(),
        "control_mode": "vehicle",
        "emergency": {"active": False, "source": ""},
        "motor": {"initialized": True, "enabled": True},
        "modules": {
            "base": "online",
            "arm": "online",
            "camera": "online",
        },
        "last_error": {"code": 0, "message": ""},
    }


def make_camera_list_response(msg, cameras):
    return {
        "type": "camera_list_response",
        "protocol_version": 1,
        "seq": msg.get("seq", 0),
        "timestamp_ms": now_ms(),
        "cameras": [make_camera_info(cam_id, url) for cam_id, url in cameras],
    }


# ============================================
# TCP 服务器
# ============================================

def send_json(conn, data):
    msg = json.dumps(data, ensure_ascii=False) + "\n"
    conn.sendall(msg.encode("utf-8"))
    msg_type = data.get("type", "?")
    # 心跳不打印
    if msg_type != "heartbeat":
        print(f"  [发送] {msg_type}: {msg.strip()[:120]}")


def handle_client(conn, addr, cameras, args):
    """处理上位机连接"""
    print(f"\n{'='*60}")
    print(f"[连接] 上位机已连接: {addr}")
    print(f"{'='*60}")

    loop_running = threading.Event()

    # 接收线程
    def recv_loop():
        try:
            buf = b""
            while True:
                data = conn.recv(4096)
                if not data:
                    break
                buf += data
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    if line.strip():
                        msg = json.loads(line)
                        msg_type = msg.get("type", "unknown")
                        if msg_type == "heartbeat":
                            send_json(conn, make_heartbeat_ack(msg))
                        elif msg_type == "operator_input":
                            keyboard = msg.get("keyboard", {})
                            pressed_keys = keyboard.get("pressed_keys", [])
                            gamepad = msg.get("gamepad", {})
                            axes = gamepad.get("axes", {})
                            buttons = gamepad.get("buttons", {})
                            active_buttons = [name for name, value in buttons.items() if value]
                            print(
                                f"  [operator_input] mode={msg.get('mode', 'unknown')} "
                                f"seq={msg.get('seq', 0)} keys={pressed_keys} "
                                f"buttons={active_buttons} axes={axes}"
                            )
                        elif msg_type == "emergency_stop":
                            print(f"  [!!!急停!!!] {msg}")
                            send_json(conn, make_ack(msg, message="emergency active"))
                        elif msg_type == "system_command":
                            print(f"  [system_command] {msg}")
                            send_json(conn, make_ack(
                                msg,
                                message=f"system command {msg.get('command', '')} accepted"
                            ))
                        elif msg_type in ("sync_request", "system_snapshot_request"):
                            send_json(conn, make_system_snapshot(msg))
                        elif msg_type == "camera_list_request":
                            send_json(conn, make_camera_list_response(msg, cameras))
                        else:
                            print(f"  [收到] {msg_type}: {str(msg)[:100]}")
        except (ConnectionResetError, ConnectionAbortedError):
            pass

    recv_thread = threading.Thread(target=recv_loop, daemon=True)
    recv_thread.start()

    # 心跳线程
    def heartbeat_loop():
        while True:
            try:
                send_json(conn, make_heartbeat())
                time.sleep(2)
            except:
                break

    hb_thread = threading.Thread(target=heartbeat_loop, daemon=True)
    hb_thread.start()

    # 持续发送线程
    def continuous_loop():
        while loop_running.wait():
            try:
                send_json(conn, make_motor_state())
                send_json(conn, make_imu_data())
                time.sleep(0.1)
            except:
                break

    loop_thread = threading.Thread(target=continuous_loop, daemon=True)
    loop_thread.start()

    try:
        send_json(conn, make_hello())
        send_json(conn, make_capabilities(cameras))

        # 初始数据
        print("\n--- 发送初始数据 ---")
        send_json(conn, make_motor_state())
        time.sleep(0.3)
        send_json(conn, make_imu_data())
        time.sleep(0.3)
        send_json(conn, make_co2_data())

        # 发送摄像头信息
        if cameras:
            time.sleep(0.5)
            print("\n--- 发送 camera_info ---")
            for cam_id, rtsp_url in cameras:
                send_json(conn, make_camera_info(cam_id, rtsp_url,
                                                  width=args.width, height=args.height,
                                                  fps=args.fps))
                time.sleep(0.2)

        print(f"\n{'='*60}")
        print("交互命令:")
        print("  回车/1 - motor_state    2 - system_status")
        print("  3 - IMU数据             4 - CO2数据")
        print("  5 - camera_info         loop - 持续发送")
        print("  stop - 停止持续发送     q - 断开")
        print(f"{'='*60}")

        while True:
            cmd = input("\n> ").strip().lower()
            if cmd == "q":
                break
            elif cmd in ("", "1"):
                send_json(conn, make_motor_state())
            elif cmd == "2":
                send_json(conn, {"type": "system_status", "cpu_temp": 55.3,
                                  "motor_driver": "OK", "battery": 75})
            elif cmd == "3":
                send_json(conn, make_imu_data())
            elif cmd == "4":
                send_json(conn, make_co2_data())
            elif cmd == "5":
                if cameras:
                    for cam_id, url in cameras:
                        send_json(conn, make_camera_info(cam_id, url,
                                                          width=args.width, height=args.height,
                                                          fps=args.fps))
                else:
                    print("  [提示] 未启用 RTSP，用 --rtsp 参数启动")
            elif cmd == "loop":
                loop_running.set()
                print("  [持续发送] 已启动 (输入 stop 停止)")
            elif cmd == "stop":
                loop_running.clear()
                print("  [持续发送] 已停止")
            else:
                send_json(conn, make_motor_state())

    except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError):
        print("\n[断开] 上位机断开连接")
    except KeyboardInterrupt:
        print("\n[中断]")
    finally:
        loop_running.clear()
        conn.close()


def main():
    parser = argparse.ArgumentParser(description="模拟ROS下位机 TCP 测试服务器")
    parser.add_argument("-p", "--port", type=int, default=9090, help="TCP 端口 (默认 9090)")
    parser.add_argument("--rtsp", action="store_true", help="同时启动 RTSP 推流")
    parser.add_argument("-n", "--num-cameras", type=int, default=1, help="摄像头数量 (默认 1)")
    parser.add_argument("--rtsp-port", type=int, default=8554, help="RTSP 端口 (默认 8554)")
    parser.add_argument("--width", type=int, default=1280, help="视频宽度")
    parser.add_argument("--height", type=int, default=720, help="视频高度")
    parser.add_argument("--fps", type=int, default=30, help="帧率")
    args = parser.parse_args()

    cameras = []
    ffmpeg_procs = []

    # 启动 RTSP 推流
    if args.rtsp:
        if check_ffmpeg():
            num = min(args.num_cameras, 5)
            print(f"\n=== 启动 {num} 路 RTSP 推流 ===")
            print(f"  提示: 需要先运行 mediamtx.exe\n")
            for i in range(num):
                proc, url = start_ffmpeg_stream(i, args.width, args.height,
                                                 args.fps, args.rtsp_port)
                ffmpeg_procs.append(proc)
                cameras.append((i, url))
                time.sleep(0.5)

    # 启动 TCP 服务器
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", args.port))
    server.listen(1)

    print(f"\n[TCP] 模拟下位机已启动，监听 0.0.0.0:{args.port}")
    print(f"[TCP] 上位机连接: 127.0.0.1:{args.port}")
    print("等待上位机连接...\n")

    def cleanup(sig=None, frame=None):
        print("\n[退出] 清理中...")
        for proc in ffmpeg_procs:
            proc.terminate()
        server.close()
        sys.exit(0)

    import signal
    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    try:
        while True:
            conn, addr = server.accept()
            handle_client(conn, addr, cameras, args)
            print("\n等待下一次连接...")
    except KeyboardInterrupt:
        cleanup()
    finally:
        server.close()


if __name__ == "__main__":
    main()
