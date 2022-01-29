#!/bin/bash

if [ "$1" == "" ]
then
  echo "Usage: $0 macroname"
  echo "Macros:"
  echo "  teams-toggle-mic     = toggle microphone input in ms-teams window"
  echo "  teams-toggle-video   = toggle video/camera in ms-teams window"
  echo "  teams-new-message    = start writing new message in extended edit mode"
  echo "  teams-exit           = end call, or cancel composing message"
  echo "  mpx-toggle-draw      = toggle drawing on screen (gromit-mpx tool)"
  echo "  mpx-clear            = clear drawings from screen (gromit-mpx tool)"
  echo "  screenshot-region    = make screenshot of region of screen"
  exit 1
fi

DONE=0

if [ "$1" == "teams-toggle-mic" ]
then
  DONE=1
  # remember current active window, focus teams, send keys "ctrl+shift+m", change focus back
  CURWIN=$(printf "0x%0x" $(xdotool getactivewindow))
  wmctrl -x -R "microsoft teams - preview"
  xdotool key ctrl+shift+m
  wmctrl -a $CURWIN -i
fi

if [ "$1" == "teams-toggle-video" ]
then
  DONE=1
  # remember current active window, focus teams, send keys "ctrl+shift+o", change focus back
  CURWIN=$(printf "0x%0x" $(xdotool getactivewindow))
  wmctrl -x -R "microsoft teams - preview"
  xdotool key ctrl+shift+o
  wmctrl -a $CURWIN -i
fi

if [ "$1" == "teams-new-message" ]
then
  DONE=1
  # focus teams, send keys "alt+shift+c" "ctrl+shift+x"
  wmctrl -x -R "microsoft teams - preview"
  xdotool key alt+shift+c
  sleep 0.2
  xdotool key ctrl+shift+x
fi

if [ "$1" == "teams-exit" ]
then
  DONE=1
  # remember current active window, focus teams, send keys "ctrl+shift+h" escape, change focus back
  # the ctrl+shift+h should en a call, and the escape should cancel starting a new message (attempt at double use of the macro-key)
  CURWIN=$(printf "0x%0x" $(xdotool getactivewindow))
  wmctrl -x -R "microsoft teams - preview"
  xdotool key ctrl+shift+h
  sleep 0.1
  xdotool key 0xff1b
  wmctrl -a $CURWIN -i
fi

if [ "$1" == "mpx-toggle-draw" ]
then
  DONE=1
  # tell gromit-mpx tool to toggle drawing (note: key 180 is a web-homepage key which I attached to gromit-mpx)
  # I start the tool on startup using: /usr/bin/gromit-mpx -K 180
  xdotool key 180
fi

if [ "$1" == "mpx-clear" ]
then
  DONE=1
  # tell gromit-mpx to clear screen (note: using a custom assigned key)
  # I start the tool on startup using: /usr/bin/gromit-mpx -K 180
  xdotool key shift+180
fi

if [ "$1" == "screenshot-region" ]
then
  DONE=1
  # make screenshot of region, and write as file with date/time in name
  /usr/bin/xfce4-screenshooter -r -s $HOME/Pictures/screenshots/screenshot-`/bin/date +"%Y%m%d-%H%M%S"`.png
  # turn off the led when done
  send-macropad-command.py d 7
fi

if [ "$DONE" != "1" ]
then
   echo "Unknown macro name '$1'"
   echo 
   $0
fi

