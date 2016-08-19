CONFIG=/boot/config.txt

sed $CONFIG -i -e "s/^dtoverlay=pisound/#dtoverlay=pisound/"
