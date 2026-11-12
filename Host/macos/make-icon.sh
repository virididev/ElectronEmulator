#!/bin/sh
# Makes the icon of a macOS application out of a drawing: make-icon.sh icon.svg icon.icns
# Needs ImageMagick; the icons are kept with the sources, so this is only for
# when a drawing changes.
set -e
SET=$(mktemp -d)/icon.iconset
mkdir "$SET"
magick -background none -density 384 "$1" -resize 1024x1024 "$SET/icon_512x512@2x.png"
for SIZE in 16 32 128 256 512; do
	sips -z $SIZE $SIZE "$SET/icon_512x512@2x.png" --out "$SET/icon_${SIZE}x${SIZE}.png" > /dev/null
done
for SIZE in 16 32 128 256; do
	sips -z $((SIZE * 2)) $((SIZE * 2)) "$SET/icon_512x512@2x.png" --out "$SET/icon_${SIZE}x${SIZE}@2x.png" > /dev/null
done
iconutil -c icns "$SET" -o "$2"
