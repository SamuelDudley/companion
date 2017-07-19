from pymavlink import mavutil
import time, threading, select, sys


filename = "last_log.tlog"

m = mavutil.mavlink_connection(filename)
m.notimestamps = False
m.timestamps_endian = '<'
m.stop_on_EOF = True

while True:
    msg = m.recv_msg()
    if msg:
        print('{0:.8f} : {1}'.format(msg._timestamp, msg))
    else:
        break