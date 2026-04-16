#!/usr/bin/env python3
# Host-side harness: launches QEMU, snapshots baseline, runs test, and restores.
import json, socket, subprocess, time

QMP_ADDR = ('127.0.0.1', 4444)              # QMP TCP socket
QEMU_CMD = [
    'qemu-system-x86_64',
    '-m', '2048',
    '-kernel', 'baremetal.bin',            # raw kernel/binary image
    '-nographic',
    '-S',                                   # start paused
    '-qmp', 'tcp:127.0.0.1:4444,server,nowait',
    '-monitor', 'none'
]

def qmp_send(sock, obj):
    data = json.dumps(obj).encode() + b'\n'
    sock.sendall(data)
    return json.loads(sock.recv(65536).decode())

def main():
    p = subprocess.Popen(QEMU_CMD, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    # wait for QMP to open
    time.sleep(0.2)
    s = socket.create_connection(QMP_ADDR)
    # handshake
    banner = s.recv(8192)
    qmp_send(s, {"execute":"qmp_capabilities"})
    # create baseline snapshot before any execution
    qmp_send(s, {"execute":"human-monitor-command",
                 "arguments":{"command-line":"savevm baseline"}})
    # start execution
    qmp_send(s, {"execute":"cont"})
    # wait for guest to signal completion (this example polls a file or uses a serial token)
    # Here we expect the guest to poweroff when done
    p.wait()
    # restore baseline for reproducible reruns
    qmp_send(s, {"execute":"human-monitor-command",
                 "arguments":{"command-line":"loadvm baseline"}})
    # optionally repeat runs by 'cont' again
    s.close()
    p.terminate()

if __name__ == '__main__':
    main()