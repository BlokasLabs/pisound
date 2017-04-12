CONFIG=/boot/config.txt

sed $CONFIG -i -e "s/^dtoverlay=pisound/#dtoverlay=pisound/"

systemctl stop pisound-btn
systemctl disable pisound-btn
