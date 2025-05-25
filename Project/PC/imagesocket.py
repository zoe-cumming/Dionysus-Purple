import socket
import numpy as np
import cv2

HOST = '172.20.10.10'
PORT = 5000
WIDTH = 240
HEIGHT = 240
FRAME_SIZE = WIDTH * HEIGHT * 2
READY_SIZE = 5

################################################
# Convert the RGB565 image data to RGB888
################################################
def rgb565_to_rgb888(frame):
    # Read the data using byteswap for little endian
    data = np.frombuffer(frame, dtype=np.uint16).byteswap().reshape((HEIGHT, WIDTH))

    r = ((data >> 11) & 0x1F) << 3
    g = ((data >> 5) & 0x3F) << 2
    b = (data & 0x1F) << 3

    r |= (r >> 5)
    g |= (g >> 6)
    b |= (b >> 5)

    rgb = np.stack((r, g, b), axis=-1).astype(np.uint8)
    return rgb

################################################
# Receive READY message and image data from
# ESP32S3 server
################################################
def recv_exact(sock, size, timeout=5):
    sock.settimeout(timeout)
    data = bytearray()
    try:
        while len(data) < size:
            remaining = size - len(data)
            packet = sock.recv(remaining)
            if not packet:
                print("Socket closed/ Connection lost")
                return None
            data.extend(packet)
        return data
    except socket.timeout:
        print("Socket timed out while waiting for data.")
        return None
    except socket.error as e:
        print(f"Socket error: {e}")
        return None

################################################
# Connect to the TCP socket created by the
# ESP32S3
# Display the images using cv2
################################################
def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    print(f"Connected to {HOST}:{PORT}")

    while True:
        ready = recv_exact(s, READY_SIZE)
        if ready != b'READY':
            print("Error:", ready)
            return
        frame = recv_exact(s, FRAME_SIZE)
        if frame is None:
            print("Connection closed / recv_exact failure")
            break

        rgb_img = rgb565_to_rgb888(frame)
        bgr_img = cv2.cvtColor(rgb_img, cv2.COLOR_RGB2BGR)
        cv2.imshow("ESP32 Frame", cv2.resize(bgr_img, (480, 480), interpolation=cv2.INTER_NEAREST))

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    s.close()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()