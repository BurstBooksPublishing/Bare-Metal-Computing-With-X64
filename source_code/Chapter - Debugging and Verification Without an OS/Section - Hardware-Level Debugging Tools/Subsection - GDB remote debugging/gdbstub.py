#!/usr/bin/env python3
# Minimal, production-quality GDB remote stub (TCP). Hook platform functions below.

import socket, binascii, struct

HOST = '0.0.0.0'
PORT = 1234

# --- Hardware hooks: replace with real implementations ---
def read_regs():
    # Return list of 64-bit registers in GDB order as Python ints.
    # Example order: RAX,RBX,RCX,RDX,RSI,RDI,RBP,RSP,R8..R15,RIP,RFLAGS
    return [0]*18

def write_regs(regs):
    # regs: iterable of 64-bit ints matching read_regs order.
    pass

def read_memory(addr, length):
    # Return bytes object of length 'length'.
    return b'\x00' * length

def write_memory(addr, data):
    # Write bytes to target memory.
    pass
# --- End hardware hooks ----------------------------------

def checksum(pkt):
    return ("%02x" % (sum(pkt) & 0xff)).encode()

def hex_encode(b):
    return binascii.hexlify(b).decode()

def hex_decode(s):
    return binascii.unhexlify(s)

def pack_regs(regs):
    # Pack little-endian 64-bit registers to hex string
    return ''.join('{:016x}'.format(r & ((1<<64)-1)) for r in regs)

def unpack_regs(hexstr):
    vals = []
    for i in range(0, len(hexstr), 16):
        vals.append(int(hexstr[i:i+16], 16))
    return vals

def handle_packet(pkt, conn):
    if pkt == b'?':
        return b'S05'  # SIGTRAP
    if pkt == b'g':
        regs = read_regs()
        return pack_regs(regs).encode()
    if pkt.startswith(b'G'):
        regs = unpack_regs(pkt[1:].decode())
        write_regs(regs)
        return b'OK'
    if pkt.startswith(b'm'):
        # mADDR,LEN
        addr_s, len_s = pkt[1:].split(b',')
        addr = int(addr_s,16); length = int(len_s,16)
        data = read_memory(addr, length)
        return hex_encode(data).encode()
    if pkt.startswith(b'M'):
        # MADDR,LEN:HEXDATA
        left, data_hex = pkt[1:].split(b':',1)
        addr_s, len_s = left.split(b',')
        addr = int(addr_s,16)
        data = hex_decode(data_hex)
        write_memory(addr, data)
        return b'OK'
    if pkt in (b'c', b'c0'):
        # continue: for bare-metal, resume target execution then block until trap
        return b'S05'
    if pkt in (b's', b's0'):
        # single-step: set TF via write_regs hook.
        return b'S05'
    if pkt.startswith(b'qSupported'):
        return b'PacketSize=4000;swbreak+;hwbreak+'
    return b''

def serve():
    with socket.create_server((HOST,PORT), reuse_port=True) as s:
        conn, addr = s.accept()
        with conn:
            conn.settimeout(10.0)
            while True:
                # Read framed packet
                ch = conn.recv(1)
                if not ch:
                    break
                if ch == b'+':  # ack from client, ignore
                    continue
                if ch != b'$':
                    continue
                payload = b''
                while True:
                    b = conn.recv(1)
                    if b == b'#':
                        break
                    payload += b
                cc = conn.recv(2)
                # validate checksum
                if checksum(payload) != cc:
                    conn.send(b'-'); continue
                conn.send(b'+')
                resp = handle_packet(payload, conn)
                if resp is None:
                    resp = b''
                out = b'$' + resp + b'#' + checksum(resp)
                conn.send(out)

if __name__ == '__main__':
    serve()