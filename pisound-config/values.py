from os import listdir, getuid, system, environ
from os.path import isfile, join, expanduser
import subprocess
import settings

def root_check():
    if getuid() != 0:
        print("Config must be run as root. Try 'sudo pisound-config'")
        exit()

def get_username():
    username = environ['SUDO_USER']
    home = expanduser('~' + environ['SUDO_USER'])
    return username

def prepare_btn_config():
    keys = ['CLICK_1', 'CLICK_2', 'CLICK_3', 
        'CLICK_OTHER', 'HOLD_1S', 'HOLD_3S', 'HOLD_5S', 'HOLD_OTHER', 'CLICK_COUNT_LIMIT']

    
    if not isfile(settings.BTN_CFG):
            with open(settings.BTN_CFG, 'w') as f:
                for key in keys:
                    f.writelines(key + '\t' + settings.BTN_SCRIPTS_DIR + '/do_nothing.sh' '\n')
    
    with open(settings.BTN_CFG, 'r') as f:
        missing_keys = []
        data = f.read()
        for key in keys:
                if str(key) not in data:
                    missing_keys.append(key)

    with open(settings.BTN_CFG, 'r') as f:
        lines = f.readlines()
    
    if len(missing_keys) > 0:
        for key in missing_keys:
            if key == 'CLICK_COUNT_LIMIT':
                lines.append(str(key + '\t' + '8' '\n'))
                continue
            lines.append(str(key + '\t' + settings.BTN_SCRIPTS_DIR + '/do_nothing.sh' '\n'))

        with open(settings.BTN_CFG, 'w') as f:
            f.writelines(sorted(lines))

prepare_btn_config()

def get_btn_config():
    prepare_btn_config()
    items = []
    with open(settings.BTN_CFG, 'r') as f:
        for line in f:
            if len(line.strip()) != 0:
                if 'UP' in line or 'DOWN' in line: 
                    continue
                try:
                    interaction = line.split()[0]
                    script_path = line.split()[1]
                    script_name = script_path.split('/')[-1]
                    name = script_name.split('.')[0].replace('_', ' ').title()
                except:
                    interaction = line.strip()
                    name = 'Not Set'
                items.append({
                    'title': interaction + ': ' + name,
                    'key': interaction,
                    'value': name
                    })
    return items

def get_btn_scripts_path():
    return settings.BTN_SCRIPTS_DIR

def get_btn_scripts():
    path = settings.BTN_SCRIPTS_DIR
    return [{'title': f.split('.')[0].replace('_', ' ').title(), 'value': join(path, f)} for f in listdir(path) if isfile(join(path, f)) and f.endswith(".sh")]

def update_btn_config(param, value):
    with open(settings.BTN_CFG, 'r') as f:
        data = f.readlines()
        for i, line in enumerate(data):
            if line.startswith(param):
                data[i] = param + '\t' + value + '\n'
                break
    with open(settings.BTN_CFG, 'w') as f:
        f.writelines(data)

def get_hs_config():
    items = []
    with open(settings.HS_CFG, 'r') as f:
        for line in f:
            for p in ['ssid', 'wpa_passphrase', 'channel']:
                if line.startswith(p):
                    param = line.strip().split('=')
                    items.append({'title': param[0] + ': ' + param[1], 'key': param[0], 'value': param[1]})
    return items

def update_hs_config(param, value):
    with open(settings.HS_CFG, 'r') as f:
        data = f.readlines()
        for i, line in enumerate(data):
            if line.startswith(param):
                data[i] = param + '=' + value + '\n'
                break
    with open(settings.HS_CFG, 'w') as f:
        f.writelines(data)

def get_serial():
    try:
        with open('/sys/kernel/pisound/serial', 'r') as f:
            data=f.read().replace('\n', '')
        return data
    except:
        return 'Pisound Not Found'

def get_version():
    try:
        with open('/sys/kernel/pisound/version', 'r') as f:
            data=f.read().replace('\n', '')
        return data
    except:
        return 'Pisound Not Found'

def get_btn_version():
    try:
        check = subprocess.check_output(['which', 'pisound-btn'])
    except:
        return 'Not Installed'
    out = subprocess.check_output(['pisound-btn', '--version'])
    return str(out.decode("utf-8")).split(' ')[1].strip(',')

def get_ctl_version():
    try:
        check = subprocess.check_output(['which', 'pisound-ctl'])
    except:
        return 'Not Installed'
    out = subprocess.check_output(['pisound-ctl', '--version'])
    return str(out.decode("utf-8")).split(' ')[4].strip(',')

def is_running_on_rpi4():
    try:
        ec = subprocess.call(['grep', '-q', 'Raspberry Pi 4', '/proc/cpuinfo'])
        return ec == 0
    except:
        return False

def get_rpi4_bootloader_update_recommendation():
    if not is_running_on_rpi4():
        return "Not running on RPi4 at the moment, cannot evaluate."
    try:
        cmd = subprocess.Popen(['vcgencmd', 'bootloader_version'], stdout=subprocess.PIPE)
        if cmd.stdout.readlines()[2].split(' ')[1].strip() < '1568112110':
            return 'Yes (See https://www.raspberrypi.org/documentation/hardware/raspberrypi/booteeprom.md)'
        else:
            return 'No'
    except:
        return "Unknown error occurred, cannot evaluate."

def is_rpi4_workaround_enabled():
    try:
        ec = subprocess.call(['grep', '-q', 'sdhci.debug_quirks2=4', '/boot/cmdline.txt'])
        return ec == 0
    except:
        return False

def set_rpi4_workaround_enabled(enabled):
    try:
        if enabled:
            if not is_rpi4_workaround_enabled():
                subprocess.call(['sed', 's/.*/& sdhci.debug_quirks2=4/', '/boot/cmdline.txt', '-i'])
        else:
            if is_rpi4_workaround_enabled():
                subprocess.call(['sed', 's/ sdhci.debug_quirks2=4//', '/boot/cmdline.txt', '-i'])
    except:
        return

def get_hw_version():
    try:
        with open('/sys/kernel/pisound/hw_version', mode='rt') as f:
            return f.read().strip('\n\0')
    except:
        return '1.0';

def is_rpi4_compatible():
    return get_hw_version() != '1.0'

def get_ip():
    out = subprocess.check_output(['hostname', '-I'])
    return str(out.decode("utf-8")).strip()

def get_hostname():
    out = subprocess.check_output(['hostname'])
    return str(out.decode("utf-8")).strip()

def get_script_path(file):
    return settings.CFG_SCRIPTS_DIR + file
