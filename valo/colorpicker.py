#!/usr/bin/env python

import sys

import serial
from PyQt4 import QtGui


arduino = serial.Serial('/dev/arduino', 9600)

def update_color(c):
    # reduce red channel, it's too bright otherwise
    r, g, b = int(0.6 * c.red()), c.green(), c.blue()
    arduino.write(bytearray([r, g, b]))


app = QtGui.QApplication(sys.argv)
dialog = QtGui.QColorDialog()
dialog.currentColorChanged.connect(update_color)
dialog.open()
sys.exit(app.exec_())
