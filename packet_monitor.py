#!/usr/bin/env python3
"""
RirePE Packet Monitor - Python CLI Client
Connects to the RirePE DLL via TCP to log and send packets
"""

import socket
import struct
import sys
import argparse
from enum import IntEnum
from datetime import datetime


class MessageHeader(IntEnum):
    """Packet message types"""
    SENDPACKET = 0
    RECVPACKET = 1
    ENCODE_BEGIN = 2
    ENCODEHEADER = 3
    ENCODE1 = 4
    ENCODE2 = 5
    ENCODE4 = 6
    ENCODE8 = 7
    ENCODESTR = 8
    ENCODEBUFFER = 9
    TV_ENCODEHEADER = 10
    TV_ENCODESTRW1 = 11
    TV_ENCODESTRW2 = 12
    TV_ENCODEFLOAT = 13
    ENCODE_END = 14
    DECODE_BEGIN = 15
    DECODEHEADER = 16
    DECODE1 = 17
    DECODE2 = 18
    DECODE4 = 19
    DECODE8 = 20
    DECODESTR = 21
    DECODEBUFFER = 22
    TV_DECODEHEADER = 23
    TV_DECODESTRW1 = 24
    TV_DECODESTRW2 = 25
    TV_DECODEFLOAT = 26
    DECODE_END = 27
    UNKNOWNDATA = 28
    NOTUSED = 29
    WHEREFROM = 30
    UNKNOWN = 31


TCP_MESSAGE_MAGIC = 0xA11CE


class PacketMonitor:
    """TCP client for monitoring packets from RirePE DLL"""

    def __init__(self, host='127.0.0.1', port=9999):
        self.host = host
        self.port = port
        self.sock = None
        self.packet_count = 0

    def connect(self):
        """Connect to the DLL's TCP server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            print(f"[+] Connected to {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"[-] Connection failed: {e}")
            return False

    def disconnect(self):
        """Disconnect from server"""
        if self.sock:
            self.sock.close()
            self.sock = None
            print("[+] Disconnected")

    def recv_message(self):
        """Receive a message from the DLL (with magic + length header)"""
        try:
            # Read magic (4 bytes)
            magic_data = self._recv_exact(4)
            if not magic_data:
                return None

            magic = struct.unpack('<I', magic_data)[0]
            if magic != TCP_MESSAGE_MAGIC:
                print(f"[-] Invalid magic: 0x{magic:08X}")
                return None

            # Read length (4 bytes)
            length_data = self._recv_exact(4)
            if not length_data:
                return None

            length = struct.unpack('<I', length_data)[0]
            if length == 0 or length > 1024 * 1024:  # 1MB max
                print(f"[-] Invalid length: {length}")
                return None

            # Read data
            data = self._recv_exact(length)
            return data

        except Exception as e:
            print(f"[-] Receive error: {e}")
            return None

    def send_message(self, data):
        """Send a message to the DLL (with magic + length header)"""
        try:
            length = len(data)
            # Pack: magic (4 bytes) + length (4 bytes) + data
            message = struct.pack('<II', TCP_MESSAGE_MAGIC, length) + data
            self.sock.sendall(message)
            return True
        except Exception as e:
            print(f"[-] Send error: {e}")
            return False

    def _recv_exact(self, n):
        """Receive exactly n bytes from socket"""
        data = b''
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                return None
            data += chunk
        return data

    def parse_packet_message(self, data):
        """Parse PacketEditorMessage structure"""
        if len(data) < 16:  # Minimum size
            return None

        # Parse header (assuming 64-bit build for now)
        # struct: header (4) + id (4) + addr (8)
        header, packet_id, addr = struct.unpack('<IIQ', data[:16])

        try:
            header_name = MessageHeader(header).name
        except ValueError:
            header_name = f"UNKNOWN_{header}"

        result = {
            'header': header,
            'header_name': header_name,
            'id': packet_id,
            'addr': addr,
            'data': data[16:]
        }

        # Parse based on message type
        if header in (MessageHeader.SENDPACKET, MessageHeader.RECVPACKET):
            if len(data) >= 20:
                pkt_length = struct.unpack('<I', data[16:20])[0]
                packet_data = data[20:20+pkt_length]
                result['packet_length'] = pkt_length
                result['packet_data'] = packet_data

        return result

    def format_hex(self, data, max_bytes=32):
        """Format binary data as hex string"""
        hex_str = ' '.join(f'{b:02X}' for b in data[:max_bytes])
        if len(data) > max_bytes:
            hex_str += '...'
        return hex_str

    def log_packet(self, msg):
        """Log a packet message to console"""
        self.packet_count += 1
        timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]

        direction = '>>>' if msg['header'] == MessageHeader.SENDPACKET else '<<<'

        print(f"\n[{self.packet_count}] {timestamp} {direction} {msg['header_name']}")
        print(f"  ID: {msg['id']}, Addr: 0x{msg['addr']:016X}")

        if 'packet_data' in msg:
            print(f"  Length: {msg['packet_length']}")
            print(f"  Data: {self.format_hex(msg['packet_data'])}")

            # Save to file if needed
            if hasattr(self, 'log_file') and self.log_file:
                self.log_file.write(f"[{self.packet_count}] {timestamp} {direction} {msg['header_name']}\n")
                self.log_file.write(f"  ID: {msg['id']}, Addr: 0x{msg['addr']:016X}\n")
                self.log_file.write(f"  Data: {msg['packet_data'].hex()}\n\n")

    def send_packet_to_dll(self, packet_data, is_recv=False):
        """Send a packet to the DLL for injection"""
        # Build PacketEditorMessage
        header = MessageHeader.RECVPACKET if is_recv else MessageHeader.SENDPACKET
        packet_id = 9999  # Custom ID
        addr = 0  # No return address

        # Pack header
        msg_header = struct.pack('<IIQ', header, packet_id, addr)

        # Pack binary data (length + packet)
        msg_data = struct.pack('<I', len(packet_data)) + packet_data

        # Send complete message
        full_message = msg_header + msg_data
        if self.send_message(full_message):
            # Wait for response
            response_data = self.recv_message()
            if response_data and len(response_data) >= 1:
                blocked = response_data[0]
                print(f"[+] Packet {'blocked' if blocked else 'allowed'}")
                return True

        return False

    def run(self, log_file=None):
        """Main monitoring loop"""
        self.log_file = None
        if log_file:
            self.log_file = open(log_file, 'w')
            print(f"[+] Logging to {log_file}")

        try:
            print("[+] Monitoring packets (Ctrl+C to stop)...")
            while True:
                data = self.recv_message()
                if not data:
                    print("[-] Connection closed")
                    break

                msg = self.parse_packet_message(data)
                if msg:
                    self.log_packet(msg)

        except KeyboardInterrupt:
            print("\n[+] Stopped by user")
        finally:
            if self.log_file:
                self.log_file.close()


def main():
    parser = argparse.ArgumentParser(description='RirePE Packet Monitor - TCP Client')
    parser.add_argument('--host', default='127.0.0.1', help='Server host (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=9999, help='Server port (default: 9999)')
    parser.add_argument('--log', help='Log file path')
    parser.add_argument('--send', help='Send a hex packet (e.g., "0A 00 01 02 03")')
    parser.add_argument('--send-recv', action='store_true', help='Send as recv packet (default: send)')

    args = parser.parse_args()

    monitor = PacketMonitor(args.host, args.port)

    if not monitor.connect():
        return 1

    try:
        if args.send:
            # Send mode
            hex_str = args.send.replace(' ', '').replace('0x', '')
            packet_data = bytes.fromhex(hex_str)
            print(f"[+] Sending packet: {monitor.format_hex(packet_data)}")
            monitor.send_packet_to_dll(packet_data, args.send_recv)
        else:
            # Monitor mode
            monitor.run(args.log)
    finally:
        monitor.disconnect()

    return 0


if __name__ == '__main__':
    sys.exit(main())
