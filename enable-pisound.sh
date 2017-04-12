CONFIG=/boot/config.txt

sed $CONFIG -i -e "s/^#dtoverlay=pisound/dtoverlay=pisound/"
if ! grep -q -E "^dtoverlay=pisound" $CONFIG; then
	printf "dtoverlay=pisound\n" >> $CONFIG
fi

sed $CONFIG -i -e "s/^#dtoverlay=i2s-mmap/dtoverlay=i2s-mmap/"
if ! grep -q -E "^dtoverlay=i2s-mmap" $CONFIG; then
	printf "dtoverlay=i2s-mmap\n" >> $CONFIG
fi

systemctl start pisound-btn
systemctl enable pisound-btn
