import socket
import struct

def send_and_receive_message(host, port, message):
    # Prepare the socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    try:
        # Encode message and prefix with 4-byte little-endian length
        message_bytes = message.encode('utf-8')
        length_prefix = struct.pack('<I', len(message_bytes))  # Little-endian
        sock.sendall(length_prefix + message_bytes)
        print(f"Sent: {message}")

        # Receive 4-byte length header for response
        length_data = recv_exact(sock, 4)
        if not length_data:
            print("No length header received.")
            return

        response_length = struct.unpack('<I', length_data)[0]
        response_data = recv_exact(sock, response_length)
        if response_data:
            print(f"Received: {response_data.decode('utf-8')}")
        else:
            print("No response data received.")
    finally:
        sock.close()

def recv_exact(sock, size):
    """Receive exactly `size` bytes from the socket."""
    buffer = b''
    while len(buffer) < size:
        data = sock.recv(size - len(buffer))
        if not data:
            return None  # Connection closed or error
        buffer += data
    return buffer

# Example usage:
send_and_receive_message('127.0.0.1', 6379, 'akram')