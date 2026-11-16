# Lists user-facing English string literals (adjacent literals joined) in given files.
import re, sys, json

LIT = r'"(?:[^"\\\n]|\\.)*"'
SEQ = re.compile(LIT + r'(?:\s*' + LIT + r')*')
CHAR = re.compile(r"'(?:[^'\\\n]|\\.)'")
COMMENT = re.compile(r'//[^\n]*')


def blank(m):
    return ' ' * len(m.group())


def sequences(s):
    """Yields (start, end, joined_text) for each run of adjacent literals."""
    s = CHAR.sub(blank, s)
    # drop // comments, but not // inside string literals (URLs)
    out, i = [], 0
    for m in re.finditer(LIT + '|' + COMMENT.pattern, s):
        if m.group().startswith('//'):
            s = s[:m.start()] + blank(m) + s[m.end():]
    for m in SEQ.finditer(s):
        txt = ''.join(re.findall(LIT[0] + r'((?:[^"\\\n]|\\.)*)"', m.group()))
        yield m.start(), m.end(), txt


if __name__ == '__main__':
    out = {}
    for f in sys.argv[1:]:
        s = open(f, encoding='utf-8').read()
        seen = []
        for start, _, txt in sequences(s):
            pre = s[s.rfind('\n', 0, start) + 1:start]
            if re.search(r'#include|qC(Debug|Warning|Info|Critical)|assert', pre):
                continue
            if not re.match(r'[A-Z][A-Za-z]', txt):
                continue
            if txt not in seen:
                seen.append(txt)
        out[f] = seen
    json.dump(out, sys.stdout, ensure_ascii=False, indent=1)


UI_CALL = re.compile(
    r'(?:SettingWidget::(?:checkbox|inverseCheckbox|dropdown|lineEdit|intInput|colorButton)'
    r'|setTooltip|setToolTip|addDescription|addTitle|addSubtitle|createCheckBox'
    r'|QLabel|QPushButton|QGroupBox|QCheckBox|setWindowTitle|setText|setPlaceholderText'
    r'|appendTab|makeButton|addRow|addDropdown|addCheckbox|tr)'
    r'(?:<[^<>()]*>)?\(\s*$'
    r'|setTitles\(\{[^;]*$'
    r'|QMessageBox::\w+\([^;]*$'
)


def is_ui(s, start):
    """True if the literal at `start` is a label/tooltip/title argument."""
    return bool(UI_CALL.search(s[max(0, start - 300):start]))


def ui_strings(s):
    return [(a, b, t) for a, b, t in sequences(s)
            if re.match(r'[A-Z][A-Za-z]', t) and is_ui(s, a)]


# Wider net for the rest of the UI (labels, buttons, menus, dialogs).
UI_CALL_WIDE = re.compile(
    r'(?:SettingWidget::\w+|setTooltip|setToolTip|addDescription|addTitle|addSubtitle'
    r'|QLabel|QPushButton|QGroupBox|QCheckBox|QAction|setWindowTitle|setText|setPlaceholderText'
    r'|addDropdown|addButton|addAction|addMenu|addTab|setTitle|addSystemMessage'
    r'|information|warning|question|critical|makeSystemMessage)'
    r'(?:<(?:[^<>]|<[^<>]*>)*>)?\((?:[^;(){}]*,)?\s*$'
    r'|QString\s+(?:text|title|label)\s*=\s*$'
)


def is_ui_wide(s, start):
    return bool(UI_CALL_WIDE.search(s[max(0, start - 300):start]))
