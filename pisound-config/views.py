import subprocess
import urwid


palette = [('body','white','black'),
    ('focustext', 'black','yellow'),
    ('field', 'white', 'dark gray'),
    ('love', 'light red, bold', 'black'),
    ('title', 'white, bold', 'black')]

def prepare(content, title='Info'):
    footer_text = 'by Blokas Community! ESC to EXIT'
    w = urwid.Frame(
        content,
        header=urwid.Text(('title', title + '\n')), 
        footer=urwid.Text(['\nwith', ('love', ' love '), footer_text])
    )
    w = urwid.Padding(w, 'center', ('relative', 80))
    w = urwid.Filler(w, 'middle', ('relative', 80))
    w = urwid.AttrMap(w, 'body')
    return w

def add_back_button(parent, title='Back'):
    button = urwid.Button(title)
    urwid.connect_signal(button, 'click', parent)
    return [urwid.AttrMap(button, 'body', focus_map='focustext'), urwid.Divider()]

def main_view(loop, title, settings, setup, info, exit):

    def add_list(items):
        for item in items:
            button = urwid.Button(item['title'])
            selection = item
            if 'callback' in item:
                callback = item['callback']
            urwid.connect_signal(button, 'click', callback, selection)
            body.append(urwid.AttrMap(button, 'body', focus_map='focustext'))

    body = []
    add_list(settings)
    body.append(urwid.Divider())
    add_list(setup)
    body.append(urwid.Divider())
    add_list(info)
    body.append(urwid.Divider())
    button = urwid.Button('Exit')
    urwid.connect_signal(button, 'click', exit)
    body.append(urwid.AttrMap(button, 'body', focus_map='focustext'))
    list_content = urwid.SimpleFocusListWalker(body)
    content = urwid.ListBox(list_content)
    loop.widget = prepare(content, title)

def list_view(loop, title, description, items, callback=False, parent=False):
    body = [urwid.Text(description), urwid.Divider()]
    if parent:
        body += add_back_button(parent)
    for item in items:
        button = urwid.Button(item['title'])
        selection = item
        if 'callback' in item:
            callback = item['callback']
        urwid.connect_signal(button, 'click', callback, selection)
        body.append(urwid.AttrMap(button, 'body', focus_map='focustext'))
    list_content = urwid.SimpleFocusListWalker(body)
    content = urwid.ListBox(list_content)
    loop.widget = prepare(content, title)

def message(loop, selection, message, parent=False):
    message = urwid.Text(message)
    body = [message, urwid.Divider()]
    if parent:
        body += add_back_button(parent)
    content = urwid.Filler(urwid.Pile(body))
    loop.widget = prepare(content)

def input(loop, selection, title, description, callback=False, parent=False):
    body = [urwid.Text(description), urwid.Divider()]
    field = urwid.Edit(multiline=False, edit_text=selection['value'])
    field_d = urwid.AttrMap(field, 'field')
    body.append(field_d)
    body.append(urwid.Divider())
    save = urwid.Button('Save')
    def save_field(button):
        selection['new_value'] = field.edit_text
        callback(button, selection)
    urwid.connect_signal(save, 'click', save_field)
    save = urwid.AttrMap(save, 'body', focus_map='focustext')
    body.append(save)
    if parent:
        body += add_back_button(parent, title='Cancel')
    list_content = urwid.SimpleFocusListWalker(body)
    content = urwid.Filler(urwid.Pile(list_content))
    loop.widget = prepare(content, title)

def run_sh(loop, selection, title, path, parent=False):
    body = []
    text = urwid.Text(' ')
    body.append(text)
    if parent or 'parent' in selection:
        if 'parent' in selection:
            parent = selection['parent']
        body += add_back_button(parent, title='Back')
    list_content = urwid.SimpleFocusListWalker(body)
    content = urwid.ListBox(list_content)
    loop.widget = prepare(content, title)
    cmd=['chmod','+x', path]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    cmd=['sh', path]
    p = subprocess.Popen(cmd, bufsize=1, universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    info = ''
    for line in iter(p.stdout.readline,''):
        line = str(line).strip()
        info = info + line + '\n'
        text.set_text(info)
        loop.draw_screen()   