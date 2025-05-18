import socket

HOST = '0.0.0.0'  # Listen on all interfaces
PORT = 12345      # Must match M5 client code

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind((HOST, PORT))
server_socket.listen()
print(f"Listening on {HOST}:{PORT}...")

conn, addr = server_socket.accept()
print(f"Connected by {addr}")

while True:
    data = conn.recv(1024)
    if not data:
        break
    print("Received:", data.decode())
    conn.sendall(b"ACK")  # Optional response

conn.close()
server_socket.close()
