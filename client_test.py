import socket
import struct

def send_command(sock, command):
    parts = command.split()  # ['SET', 'foo', 'bar']
    nstr = len(parts)
    payload = struct.pack("<I", nstr)  # little endian uint32

    for part in parts:
        encoded = part.encode('utf-8')
        payload += struct.pack("<I", len(encoded)) + encoded

    sock.sendall(payload)
    print(f"Sent: {command}")

def receive_response(sock):
    # First, read 4 bytes for status code
    raw_status = sock.recv(4)
    if len(raw_status) < 4:
        print("[ERROR] Incomplete response status")
        return

    status_code = struct.unpack("<I", raw_status)[0]

    # Read the rest (assume a small string response)
    # You can customize how much to read if needed.
    response_data = sock.recv(1024).decode('utf-8', errors='ignore')  # loose read

    print(f"Response Status: {status_code}")
    print(f"Response Data: {response_data}")

def main():
    host = "127.0.0.1"
    port = 6379

    with socket.create_connection((host, port)) as sock:
        send_command(sock, "SET oof rar")
        send_command(sock, "GET oof")
        send_command(sock, "SET A B")
        # No receive yet → both commands sent in one go (pipelined)

        receive_response(sock)
        receive_response(sock)


        #send_command(sock, "GET foo")
        #receive_response(sock)

if __name__ == "__main__":
    main()
