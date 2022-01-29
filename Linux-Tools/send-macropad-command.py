#!/usr/bin/python3

"""
send-macropad-command.py

This tool can be used to interact with the macro key pad from within batch scripts,
or from the shell/bash command line.

Type:
- "send-macropad-command.py -h" for help about this tool.
- "send-macropad-command.py help" for help about the macro pad commands.

This last help, needs a functioning macro pad connected, and proper detection of
the com port / device to use. If auto detection does not work, execute:

"send-macropad-command.py -v -l" to debug it.

Fixing auto detect can be done either by using the -d option, or by finding
some unique words in the "-l" data-set, and passing these in via "-a".

Thijs Kaper, January 29, 2022.
"""

# Silence some pylint warnings (non-standard names or globals, too general exception catching)
#pylint:disable=C0103,W0703,W0603

import argparse
import re
import textwrap
import sys
import threading
import serial
import serial.tools.list_ports

# Define command line argument parser
parser = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description='Send command to macro keypad',
    epilog=textwrap.dedent('''\
        Note: if you use the 'f' or 'flash' command, you need to increase the timeout if you also
        need a response to be read. Count each flash time double (one for on, one for off time).

        Examples:
        %(prog)s -v -l                       # debug port autodetection
        %(prog)s -a "9206 hidpc" -l -v       # change auto detect keywords to find proper macro pad device
        %(prog)s help                        # get command help
        %(prog)s -t 0.3 help                 # get command help, use increased timeout on slow computer
        %(prog)s -t 0.5 t 2 f 1 200 e 2 g 1  # toggle 2, flash 1 for 200ms, and turn on 2 after that, read 1
        %(prog)s -d /dev/ttyACM0 t 1         # toggle led 1, use device /dev/ttyACM0 instead of autodetecting the device
        _
        ''')
    )
parser.add_argument(dest='commands', metavar='command', nargs='*')
parser.add_argument('-i', '--interactive', dest='interactive', action='store_true',
                    help='interactive mode (end with "exit" or ctrl-d)')
parser.add_argument('-v', '--verbose', dest='verbose', action='store_true',
                    help='verbose / debug mode')
parser.add_argument('-q', '--quiet', dest='quiet', action='store_true', help='quiet / silent mode')
parser.add_argument('-l', '--list', dest='list', action='store_true', help='list serial ports')
parser.add_argument('-t', '--timeout', metavar='timeout', type=float, dest='timeout', default=0.1,
                    help='time to wait for macro keypad to send a response to the command'
                    ' (default %(default)s, if partial or no response, change to 0.2 or higher)')
parser.add_argument('-b', '--baud-rate', metavar='bps', type=int, dest='baud_rate', default=115200,
                    help='baud rate, not interesting for atmega-32u4, but might be for others'
                    ' (default %(default)s)')
group = parser.add_mutually_exclusive_group()
group.add_argument('-d', '--device', metavar='device', dest='device',
                   help='serial device to use (example /dev/ttyACM0)')
group.add_argument('-a', '--autodetect', metavar='matchwords', dest='autodetect',
                   default='1B4F:9206 SparkFun',
                   help='auto detect serial port hwid/description words (default: %(default)s)')
args = parser.parse_args()

def debug(data):
    """Debug / verbose conditional print function (enable with -v)"""
    if args.verbose:
        print("# " + data)

# Autodetect the port to use. Will be overwritten later, if someone specified the -d flag.
autodetectregex = ".*" + (args.autodetect.lower()).replace(" ", ".*") + ".*"
comportlist = serial.tools.list_ports.comports()
device = None
debug("Autodetect search regex: " + autodetectregex)
for element in comportlist:
    finder = (str(element.hwid) + " " + str(element.description)).lower()
    debug("For device " + element.device + "; Autodetect string to match against: " + finder)
    if re.match(autodetectregex, finder):
        debug("Match: autodetected device: " + element.device)
        if device and not args.device:
            print("WARNING: multiple matches on autodetect. Last match will be used."
                  "Use -v and -l options to investigate, and change -a or -d value to fix this!")
            print("Previous match: " + device + ", current match: " + element.device)
        device = element.device

# If the -l option was used, show available serial ports,
# and show which one would be chosen from the autodetect keywords.
if args.list:
    print("The following serial ports are found:\n")
    for element in comportlist:
        print("device:" + element.device + "\n    description:" + str(element.description) +
              "\n    hwid:" + str(element.hwid) + "\n")
    print("Based on the autodetect setting keywords, we would choose: " + str(device))
    sys.exit(0)

# Override device when -d defined.
if args.device:
    debug("Overriding autodetected device " + str(device) + " with " + args.device)
    device = args.device

debug("Device to use: " + device + ", " + str(args.baud_rate) + " baud")

# Check if the user specified either any commands to send or to use interactive mode.
if (not args.commands) != args.interactive:
    print("Either specify a command as argument, or use -i for interactve mode.\n")
    parser.print_help()
    sys.exit(1)

# Open the serial port.
try:
    ser = serial.Serial(device, args.baud_rate, timeout=args.timeout)
except Exception as msg:
    print("Error opening serial port " + device + ", error: " + str(msg))
    sys.exit(1)

# Flag for use in interactive mode to indicate (not) end of processing.
keep_reading = True

# response read function
def read_response():
    """Try to read response, catch any excpetions."""
    global ser, keep_reading
    try:
        response = ser.read(size=2048).decode('utf-8').strip()
    except Exception as msg:
        print("Error reading response: " + str(msg))
        keep_reading = False
        return None
    return response

# reader thread function
def response_reader_thread():
    """Routine for background thread to keep reading and printing data as read from serial port."""
    global keep_reading
    debug("Start background thread for reading responses")
    while keep_reading:
        response = read_response()
        if response:
            print(response)
    debug("End background thread for reading responses")

# If user passed in "-i" option, we are using interactive mode.
# Keep asking for user input, and start a response reader thread in the background.
if args.interactive:
    t1 = threading.Thread(target=response_reader_thread)
    t1.start()
    debug("Start interactive mode, reading stdin until end-of-file (ctrl-d, or type exit)")
    for line in sys.stdin:
        line = line.strip()
        if line == "exit" or not keep_reading:
            debug("Stopping")
            break
        debug("Send: " + line)
        ser.write((line + "\n").encode('utf-8'))
    debug("End of interactive mode")
    keep_reading = False
    t1.join()
    sys.exit(0)

# Non-interactive mode, send command from commmand line arguments
commandstring = ' '.join(args.commands) + "\n"
debug("Send command: " + commandstring.strip())

# Send commands
ser.write(commandstring.encode('utf-8'))

debug("Read response (reading for " + str(args.timeout) + " seconds):")
# Read response
answer = read_response()

# And print any responses (if not -q / -quiet)
if not args.quiet:
    if answer:
        print(answer)
    else:
        debug("No response")
else:
    debug("Quiet mode, do not print answer")

ser.close()
