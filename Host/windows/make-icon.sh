#!/bin/sh
# Makes the icon of the Windows program out of a drawing: make-icon.sh icon.svg icon.ico
# Needs ImageMagick; the icons are kept with the sources, so this is only for
# when a drawing changes.
set -e
magick -background none -density 384 "$1" -define icon:auto-resize=256,128,64,48,32,24,16 "$2"
