#! /usr/bin/python3

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
        {'title': 'Change Pisound Hotspot Settings', 'callback': hs_menu},
        {'title': 'Change Default System Soundcard', 'callback': cards_menu}]
    setup = [{'title': 'Install Additional Software', 'callback': install_menu},
        {'title': 'Update Pisound', 'parent': restart, 'callback': run_sh, 'file': 'system-update.sh'}]
    info = [{'title': 'Show More Info', 'callback': info_message}]
    view = views.main_view(loop, title, settings, setup, info, exit)

def btn_menu(button, *selection):
    title = 'Pisound Button Settings'
    description = "Here you can assign different "\
        "actions to different Button interactions. We know it sounds funny."\
        "\n\n'OTHER_CLICKS' - when 4 and more consecutive clicks are received."\
        "\n'HOLD_OTHER' - when pressed for 7 and more seconds."
    items = values.get_btn_config()
    callback = btn_action_menu
    parent = main_menu
    view = views.list_view(loop, title, description, items, callback, parent)

def btn_action_menu(button, selection):
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
    values.update_btn_config(selection['key'], selection['value'])
    btn_menu(selection)

def cards_menu(button, *selection):
    title = 'Change Default Card'
    active_card = values.get_active_card()
    description = 'Currently active card is {}'.format(active_card['title'])
    items = values.get_cards()
    for item in items:
        item['current'] = active_card['key']
    parent = main_menu
    callback = set_card_silent
    view = views.list_view(loop, title, description, items, callback, parent=parent)

def set_card_silent(button, selection):
    values.set_active_card(selection)
    cards_menu(selection)

def hs_menu(button, *selection):
    title = 'Pisound Hotspot Settings'
    description = 'Here you can change Pisound Hotspot name, password and channel.'
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
        hs_restart_message(selection)

def hs_restart_message(selection):
    message = 'The changes will take an effect next time you start the hotspot.'
    parent = hs_menu
    view = views.message(loop, selection, message, parent)

def info_message(button, selection):
    version = values.get_version()
    serial = values.get_serial()
    btn_version = values.get_btn_version()
    ctl_version = values.get_ctl_version()
    ip = values.get_ip()
    hostname = values.get_hostname()
    message = 'Button Version: {}'\
        '\nServer Version: {}\nFirmware Version: {}'\
        '\nSerial Number: {}\nIP Address: {}'\
        '\nHostname: {}\n'.format(btn_version, ctl_version, version, serial, ip, hostname)
    parent = main_menu
    view = views.message(loop, selection, message, parent)

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