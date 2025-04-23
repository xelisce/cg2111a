#!/usr/bin/env bash

lxterminal -e "python ~/Desktop/rosnslam/labs/SlamLab/alex_main.py" &
lxterminal -e "python ~/Desktop/Appendix-UsingYourCamera/AlexCameraStreamServer.py" &
lxterminal -e "~/Desktop/cg2111a/alex/alex/alex-pi" &

