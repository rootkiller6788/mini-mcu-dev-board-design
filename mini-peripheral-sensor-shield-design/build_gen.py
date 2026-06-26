
import os, sys
BASE = os.path.dirname(os.path.abspath(__file__))
total = 0

def w(path, content):
    global total
    full = os.path.join(BASE, path)
    d = os.path.dirname(full)
    if d and not os.path.isdir(d):
        os.makedirs(d, exist_ok=True)
    with open(full, 'w', encoding='utf-8', newline='
') as f:
        f.write(content)
    lines = len(content.splitlines()) if content else 0
    total += lines
    print(f'  Wrote: {path}  ({lines} lines)')
    sys.stdout.flush()
    return lines

print('=== BUILD START ===')

