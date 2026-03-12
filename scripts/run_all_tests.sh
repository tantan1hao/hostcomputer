#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/test-logs"
mkdir -p "$LOG_DIR"
TS="$(date +%Y%m%d_%H%M%S)"
REPORT="$LOG_DIR/run_all_${TS}.txt"
RESTART_ROUNDS="${RESTART_ROUNDS:-10}"
FUZZ_SEC="${FUZZ_SEC:-12}"
STRESS_SEC="${STRESS_SEC:-600}"

log() { echo "[$(date +%H:%M:%S)] $*" | tee -a "$REPORT"; }

log "A1) smoke test"
"$ROOT/scripts/smoke_test.sh" | tee -a "$REPORT"

log "A2) restart 10x"
RESTART_ROUNDS="$RESTART_ROUNDS" python3 - <<'PY' | tee -a "$REPORT"
import subprocess, time
import os
r=int(os.getenv('RESTART_ROUNDS','10'))
ok=0
for i in range(r):
    p=subprocess.Popen(['./build-openclaw-nosdk/hostcomputer'], cwd='.')
    time.sleep(3)
    alive=(p.poll() is None)
    if alive:
        ok += 1
        p.terminate()
        try:p.wait(timeout=2)
        except: p.kill(); p.wait()
print(f'RESTART_OK {ok}/{r}')
PY

log "A3) udp fuzz runtime 12s"
FUZZ_SEC="$FUZZ_SEC" python3 - <<'PY' | tee -a "$REPORT"
import subprocess, time, socket, os, random
p=subprocess.Popen(['./build-openclaw-nosdk/hostcomputer'])
time.sleep(2)
start=time.time()
s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sent=0
dur=float(os.getenv('FUZZ_SEC','12'))
while time.time()-start<dur:
    s.sendto(os.urandom(random.choice([0,1,2,3,4,5,6,8,15,32,64])),('127.0.0.1',9700)); sent+=1
    for port in range(5000,5006):
        s.sendto(os.urandom(random.choice([5,13,27,40,77])),('127.0.0.1',port)); sent+=1
    time.sleep(0.02)
alive=(p.poll() is None)
if alive:
    p.terminate()
    try:p.wait(timeout=3)
    except: p.kill(); p.wait()
print(f'FUZZ_SENT {sent}')
print(f'ALIVE_DURING_FUZZ {alive}')
print(f'EXIT_CODE {p.returncode}')
PY

log "A4) 10min stress"
STRESS_SEC="$STRESS_SEC" python3 - <<'PY' | tee -a "$REPORT"
import subprocess, time
import os
p=subprocess.Popen(['./build-openclaw-nosdk/hostcomputer'])
start=time.time()
dur=float(os.getenv('STRESS_SEC','600'))
while time.time()-start<dur:
    if p.poll() is not None:
        break
    time.sleep(1)
alive=(p.poll() is None)
if alive:
    p.terminate()
    try:p.wait(timeout=5)
    except: p.kill(); p.wait()
print(f'ALIVE_10MIN {alive}')
print(f'EXIT_CODE {p.returncode}')
PY

log "B) mock ROS1 server script ready: scripts/mock_ros1_server.py"
log "C) async tcp patch check:"
if rg -n "waitForConnected|waitForDisconnected" "$ROOT/src/communication/ROS1TcpClient.cpp" >/dev/null; then
  log "WARN: blocking wait still exists"
else
  log "PASS: no blocking wait calls found"
fi

log "DONE. report=$REPORT"
