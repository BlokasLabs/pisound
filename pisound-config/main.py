#!/usr/bin/env python3

import subprocess
import os
import sys
import urwid
import views
import values

def show_or_exit(key):
    if key in ('esc', ):
        raise urwid.ExitMainLoop()

def exit(button, *selection):
    raise urwid.ExitMainLoop()

def restart(button, *selection):
    loop.stop()
    os.execl(sys.executable, *([sys.executable]+sys.argv))

def add_back_button(parent, title='Back'):
    button = urwid.Button(title)
    urwid.connect_signal(button, 'click', parent)
    return [urwid.AttrMap(button, 'body', focus_map='focustext'), urwid.Divider()]

def main_menu(*button):
    title = 'Pisound Configuration Tool'
    settings = [{'title': 'Change Pisound Button Settings', 'callback': btn_menu},
        {'title': 'Change Pisound Hotspot Settings', 'callback': hs_menu}]
    setup = [{'title': 'Install Additional Software', 'callback': install_menu},
        {'title': 'Update Pisound', 'parent': restart, 'callback': run_sh, 'file': 'system-update.sh'}]

    setup.append({'title': 'Raspberry Pi 4 Compatibility', 'callback': rpi4_compatibility_menu})

    info = [{'title': 'Show More Info', 'callback': info_message}]
    view = views.main_view(loop, title, settings, setup, info, exit)

def btn_menu(button, *selection):
    title = 'Pisound Button Settings'
    description = "Here you can assign different "\
        "actions to different Button interactions. We know it sounds funny."\
        "\n\n'OTHER_CLICKS' - when 4 and more consecutive clicks are received."\
        "\n'HOLD_OTHER' - when pressed for 7 and more seconds."\
        "\n'CLICK_COUNT_LIMIT' - the maximum press count limit. Use 0 for no limit."
    items = values.get_btn_config()
    callback = btn_action_menu
    parent = main_menu
    view = views.list_view(loop, title, description, items, callback, parent)

def btn_param_menu(button, selection):
    title = "Change '{}' value".format(selection['key'])
    description = 'Enter a new value bellow:'
    callback = btn_update_silent
    parent = btn_menu
    view = views.input(loop, selection, title, description, callback, parent)

def btn_action_menu(button, selection):
    if selection['key'] == 'CLICK_COUNT_LIMIT':
        btn_param_menu(button, selection)
        return
    interaction = selection['key']
    title = "Button '{}' Action".format(interaction)
    description = "To assign your own script, place it inside '{}' directory.".format(
            values.get_btn_scripts_path())
    items = values.get_btn_scripts()
    for item in items:
        item['key'] = interaction
    callback = btn_update_silent
    parent = btn_menu
    view = views.list_view(loop, title, description, items, callback, parent)

def btn_update_silent(button, selection):
    val = selection['value']
    if 'new_value' in selection:
        val = selection['new_value']
    values.update_btn_config(selection['key'], val)
    btn_menu(selection)

def hs_menu(button, *selection):
    title = 'Pisound Hotspot Settings'
    description = 'Here you can change Pisound Hotspot name and password.'
    items = values.get_hs_config()
    callback = hs_param_menu
    parent = main_menu
    widget = views.list_view(loop, title, description, items, callback, parent)

def hs_param_menu(button, selection):
    title = "Change '{}' value".format(selection['key'])
    description = 'Enter a new value bellow:'
    callback = hs_update_silent
    parent = hs_menu
    view = views.input(loop, selection, title, description, callback, parent) 

def hs_update_silent(button, selection):
    if selection['new_value'] == selection['value']:
        hs_menu(selection)
    else:
        values.update_hs_config(selection['key'], selection['new_value'])
        hs_restart_message(selection['key'])

def toggle_rpi4_workaround(button, selection):
    values.set_rpi4_workaround_enabled(not values.is_rpi4_workaround_enabled())
    views.message(loop, 'Done!', 'Reboot the system for the change to take effect.', main_menu)

def rpi4_compatibility_menu(button, selection):
    title = 'Raspberry Pi 4 Compatibility'

    description_long = """Raspberry Pi 4 has broken the behavior of the 3.3V line on the 40-pin GPIO header. This causes an issue on reboot which is described here:

https://community.blokas.io/t/pisound-with-raspberry-pi-4/1238/12

You may use the options below to enable or disable a software workaround to avoid the issue. In particular, it disables the '1.8V' communication mode of the SD card. It has a downside that it limits the SD card speeds to the ones available on Raspberry Pi 3, which are still quite fast. You'd be impacted only if you use one of the high end SD cards, but then you could opt to instead have the workaround off and keep the reboot issue in mind.

The software workaround gets applied to /boot/cmdline.txt - it has to be re-applied every time you format the SD card.

Since hardware version 1.1, Pisound has a built-in hardware workaround to fix the compatibility with RPi 4.

----

Bootloader EEPROM Update Recommended: {}

Software Workaround Enabled: {}"""

    if not values.is_rpi4_compatible():
        description = description_long.format(values.get_rpi4_bootloader_update_recommendation(), 'Yes' if values.is_rpi4_workaround_enabled() else 'No')
    else:
        description = 'Your Pisound is fully compatible with RPi 4.'
        if values.is_rpi4_workaround_enabled():
            description += "\n\nThe software workaround is enabled (https://community.blokas.io/t/pisound-with-raspberry-pi-4/1238/12), it's recommended to disable it."
        else:
            description += "\n\nThe software workaround is disabled (https://community.blokas.io/t/pisound-with-raspberry-pi-4/1238/12), it's recommended to keep it disabled."

    callback=toggle_rpi4_workaround

    items = [{'title': 'Enable Workaround' if not values.is_rpi4_workaround_enabled() else 'Disable Workaround'}]
    view = views.list_view(loop, title, description, items, callback, main_menu)

def hs_restart_message(selection):
    message = 'The changes will take an effect next time you start the hotspot.'
    parent = hs_menu
    view = views.message(loop, selection, message, parent)

def info_message(button, selection):
    version = values.get_version()
    serial = values.get_serial()
    btn_version = values.get_btn_version()
    ctl_version = values.get_ctl_version()
    hw_version = values.get_hw_version()
    ip = values.get_ip()
    hostname = values.get_hostname()
    message = 'Button Version: {}'\
        '\nServer Version: {}\nFirmware Version: {}\nHardware Version: {}'\
        '\nSerial Number: {}\nIP Address: {}'\
        '\nHostname: {}\n'.format(btn_version, ctl_version, version, hw_version, serial, ip, hostname)
    parent = main_menu
    view = views.message(loop, 'Info', message, parent)

def install_menu(button, *selection):
    title = 'Install Additional Software'
    description = 'Choose software package to install/update.'
    callback = run_sh
    items = [{'title': 'PureData', 'file': 'install-puredata.sh'},
        {'title': 'Audacity', 'file': 'install-audacity.sh'},
        {'title': 'SuperCollider', 'file': 'install-supercollider.sh'},
        {'title': 'TouchOSC2MIDI', 'file': 'install-touchosc2midi.sh'}]
    parent = main_menu
    view = views.list_view(loop, title, description, items, callback, parent)

def run_sh(button, selection):
    selection['path'] = values.get_script_path(selection['file'])
    view = views.run_sh(loop, selection, selection['title'], selection['path'], parent=install_menu)

values.root_check()
widget = urwid.SolidFill()
loop = urwid.MainLoop(widget, views.palette, unhandled_input=show_or_exit)
main_menu()
loop.run()
