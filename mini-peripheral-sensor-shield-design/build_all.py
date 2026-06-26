import os, sys
BASE = os.path.dirname(os.path.abspath(__file__))
total_lines = 0

def w(path, content):
    """Write file and return line count"""
    global total_lines
    full = os.path.join(BASE, path)
    d = os.path.dirname(full)
    if d and not os.path.isdir(d):
        os.makedirs(d, exist_ok=True)
    with open(full, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    lines = content.count("\n") + 1 if content else 0
    total_lines += lines
    print(f"  {path}: {lines} lines")
    return lines

print("=== Building mini-peripheral-sensor-shield-design ===\n")
sys.stdout.flush()