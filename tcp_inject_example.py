#!/usr/bin/env python3
"""
TCP Packet Injection Example
Demonstrates: Receive RECV packet → Send SEND packet → Disconnect

This example shows how to:
1. Connect to the DLL TCP server
2. Wait for a RECVPACKET from the game server
3. Respond by injecting a SENDPACKET to the game server
4. Disconnect gracefully
"""

import socket
import struct
import sys

TCP_MESSAGE_MAGIC = 0xA11CE

# Message types (from RirePE.h MessageHeader enum)
SENDPACKET = 0
RECVPACKET = 1

class RirePETCPClient:
    def __init__(self, host='127.0.0.1', port=9999):
        self.host = host
        self.port = port
        self.sock = None

    def connect(self):
        """Connect to DLL TCP server"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        print(f"[✓] Connected to {self.host}:{self.port}")

    def recv_message(self):
        """Receive framed TCP message from DLL"""
        # Read frame header (8 bytes: magic + length)
        header = self._recv_exact(8)
        if not header:
            return None

        magic, length = struct.unpack('<II', header)

        # Verify magic
        if magic != TCP_MESSAGE_MAGIC:
            raise ValueError(f"Invalid magic: 0x{magic:X}, expected 0x{TCP_MESSAGE_MAGIC:X}")

        # Read payload
        data = self._recv_exact(length)
        return data

    def send_inject_packet(self, header_type, packet_bytes):
        """
        Send packet injection command to DLL

        Args:
            header_type: SENDPACKET (0) or RECVPACKET (1)
            packet_bytes: Raw packet data including header
        """
        # Build PacketEditorMessage structure
        # struct PacketEditorMessage {
        #     DWORD header;           // 4 bytes - message type
        #     DWORD id;               // 4 bytes - packet ID (can be 0)
        #     ULONG_PTR addr;         // 8 bytes - return address (can be 0)
        #     struct {
        #         DWORD length;       // 4 bytes - packet size
        #         BYTE packet[1];     // N bytes - packet data
        #     } Binary;
        # }

        # Pack: header(4) + id(4) + addr(8) + length(4) + packet
        message = struct.pack('<IIQI',
            header_type,          # header (SENDPACKET or RECVPACKET)
            0,                    # id (not used for injection)
            0,                    # addr (not used for injection)
            len(packet_bytes)     # Binary.length
        ) + packet_bytes          # Binary.packet

        # Wrap in TCPMessage frame
        frame = struct.pack('<II', TCP_MESSAGE_MAGIC, len(message)) + message

        self.sock.sendall(frame)
        print(f"[→] Sent {header_type} injection command ({len(packet_bytes)} bytes)")

    def parse_packet_message(self, data):
        """Parse PacketEditorMessage from received data"""
        if len(data) < 16:
            return None

        # Parse header: header(4) + id(4) + addr(8) = 16 bytes
        header_type = struct.unpack('<I', data[0:4])[0]
        packet_id = struct.unpack('<I', data[4:8])[0]
        addr = struct.unpack('<Q', data[8:16])[0]

        result = {
            'header': header_type,
            'id': packet_id,
            'addr': addr
        }

        # Parse payload based on message type
        if header_type in [SENDPACKET, RECVPACKET]:
            length = struct.unpack('<I', data[16:20])[0]
            packet_data = data[20:20+length]
            result['binary'] = {
                'length': length,
                'packet': packet_data
            }
        else:  # Format messages
            pos = struct.unpack('<I', data[16:20])[0]
            size = struct.unpack('<I', data[20:24])[0]
            update = struct.unpack('<I', data[24:28])[0]
            extra_data = data[28:28+size] if update == 1 else None
            result['extra'] = {
                'pos': pos,
                'size': size,
                'update': update,
                'data': extra_data
            }

        return result

    def disconnect(self):
        """Close connection"""
        if self.sock:
            self.sock.close()
            print("[✓] Disconnected")

    def _recv_exact(self, n):
        """Receive exactly n bytes"""
        data = b''
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                return None
            data += chunk
        return data


def main():
    """
    Example workflow:
    1. Wait for RECVPACKET from game server
    2. When specific packet arrives, respond with SENDPACKET
    3. Disconnect and exit
    """

    # Configuration
    HOST = '127.0.0.1'
    PORT = 9999

    # Target packet header to watch for (example: 0x1234)
    TARGET_RECV_HEADER = 0x1234

    # Response packet to send (example: header 0x5678 + some data)
    RESPONSE_PACKET = struct.pack('<H', 0x5678) + b'\x01\x02\x03\x04'

    print("=" * 60)
    print("TCP Packet Injection Example")
    print("=" * 60)
    print(f"Target: {HOST}:{PORT}")
    print(f"Watching for RECVPACKET with header 0x{TARGET_RECV_HEADER:04X}")
    print(f"Will respond with SENDPACKET: {RESPONSE_PACKET.hex()}")
    print("=" * 60)

    client = RirePETCPClient(HOST, PORT)

    try:
        # Step 1: Connect
        client.connect()

        # Step 2: Monitor incoming packets
        print("\n[⏳] Waiting for packets from game server...")

        while True:
            # Receive message from DLL
            data = client.recv_message()
            if not data:
                print("[!] Connection closed by server")
                break

            # Parse the message
            msg = client.parse_packet_message(data)
            if not msg:
                continue

            # Handle different message types
            if msg['header'] == SENDPACKET:
                # Outgoing packet (game → server)
                packet_hex = msg['binary']['packet'].hex()
                print(f"[←] SENDPACKET #{msg['id']}: {packet_hex}")

            elif msg['header'] == RECVPACKET:
                # Incoming packet (server → game)
                packet_data = msg['binary']['packet']
                packet_hex = packet_data.hex()

                # Extract header (first 2 bytes, little-endian)
                if len(packet_data) >= 2:
                    packet_header = struct.unpack('<H', packet_data[0:2])[0]
                    print(f"[→] RECVPACKET #{msg['id']}: Header=0x{packet_header:04X} Data={packet_hex}")

                    # Check if this is our target packet
                    if packet_header == TARGET_RECV_HEADER:
                        print(f"\n[!] Target packet detected (0x{TARGET_RECV_HEADER:04X})!")
                        print(f"[!] Injecting response packet...")

                        # Step 3: Inject response SENDPACKET
                        client.send_inject_packet(SENDPACKET, RESPONSE_PACKET)

                        print(f"[✓] Response packet injected: {RESPONSE_PACKET.hex()}")

                        # Step 4: Disconnect and exit
                        print(f"\n[!] Mission complete, disconnecting...")
                        break
                else:
                    print(f"[→] RECVPACKET #{msg['id']}: {packet_hex}")

            else:
                # Format messages (ENCODE/DECODE)
                if 'extra' in msg:
                    print(f"[📊] Format message type={msg['header']} "
                          f"pos={msg['extra']['pos']} size={msg['extra']['size']}")

    except KeyboardInterrupt:
        print("\n[!] Interrupted by user")

    except Exception as e:
        print(f"[✗] Error: {e}")
        import traceback
        traceback.print_exc()

    finally:
        client.disconnect()
        print("\n[✓] Example completed")


if __name__ == '__main__':
    main()
