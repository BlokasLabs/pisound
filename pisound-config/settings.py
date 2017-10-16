import os

path = os.path.dirname(os.path.realpath(__file__))

CFG_SCRIPTS_DIR = os.environ.get("CFG_SCRIPTS_DIR", path + '/scripts/')
BTN_SCRIPTS_DIR = os.environ.get("BTN_SCRIPTS", '/usr/local/pisound/scripts/pisound-btn')
BTN_CFG = os.environ.get("BTN_CFG", '/etc/pisound.conf')
HS_CFG = os.environ.get("HS_CFG", '/usr/local/pisound/scripts/pisound-btn/hostapd.conf')
