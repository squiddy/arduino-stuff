import colorsys
import socket
import struct
from collections import namedtuple, deque

import serial


Packet = namedtuple('Packet', 'channels y1 v1 u1')
PacketStruct = 'BBBB'


def moving_average(period):
    stream = deque()
    while True:
        length = len(stream)

        if length > period:
            stream.popleft()
            length -= 1

        if length == 0:
            average = 0
        else:
            average = sum(stream) / length

        data = yield average
        stream.append(data)


clamp = lambda x: int(max(0, min(255, x)))

def yuv_to_rgb(y, v, u):
    # formular from http://en.wikipedia.org/wiki/YUV#Y.27UV444
    b = 298 * (y - 16) + 516 * (v - 128)
    g = 298 * (y - 16) - 100 * (v - 128) - 208 * (u - 128)
    r = 298 * (y - 16)                   + 409 * (u - 128)

    return clamp((r + 128) / 256), clamp((g + 128) / 256), clamp((b + 128) / 256)


def adjust_color(r, g, b):
    # Increase saturation, decrease brightness
    (h, s, v) = colorsys.rgb_to_hsv(r / 256.0, g / 256.0, b / 256.0)
    s = max(0, min(1, s * 1.6))
    v = max(0, min(1, v * 0.3))
    (r, g, b) = [int(x * 255) for x in colorsys.hsv_to_rgb(h, s, v)]

    # Tone down red channel, it's too bright otherwise
    r = int(r * 0.6)

    return clamp(r), clamp(g), clamp(b)


def main(): 
    arduino = serial.Serial('/dev/arduino', 9600)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 5005))

    avg_red = moving_average(5)
    avg_red.send(None)
    avg_green = moving_average(5)
    avg_green.send(None)
    avg_blue = moving_average(5)
    avg_blue.send(None)

    while True:
        data, addr = sock.recvfrom(struct.calcsize(PacketStruct))
        p = Packet(*struct.unpack(PacketStruct, data))

        r, g, b = yuv_to_rgb(p.y1, p.v1, p.u1)
        r, g, b = [avg_red.send(r), avg_green.send(g), avg_blue.send(b)]
        r, g, b = adjust_color(r, g, b)

        arduino.write(bytearray([r, g, b]))
        

if __name__ == '__main__': 
    main()
