"""Keep upstream Waypaper's UI; make mpv IPC safe and session-private."""
from pathlib import Path
import sys
p=Path(sys.argv[1])/'waypaper/changer.py'
s=p.read_text()
start=s.index('def change_with_mpvpaper(')
end=s.index('\ndef change_with_gslapper(',start)
replacement='''def change_with_mpvpaper(image_path: Path, cf: Config, monitor: str):
    """Sovel compatibility patch: JSON IPC; no filenames passed to a shell."""
    import json
    runtime = os.environ.get("XDG_RUNTIME_DIR")
    if not runtime:
        raise RuntimeError("A graphical login runtime directory is required")
    socket_path = Path(runtime) / ("waypaper-mpv-" + hashlib.sha256(monitor.encode()).hexdigest()[:12] + ".sock")
    if socket_path.exists():
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
                connection.settimeout(2)
                connection.connect(str(socket_path))
                connection.sendall((json.dumps({"command": ["loadfile", str(image_path)]}) + "\\n").encode())
            return
        except OSError:
            socket_path.unlink(missing_ok=True)
    fill = {"fill": "panscan=1.0", "fit": "panscan=0.0", "center": "", "stretch": "keepaspect=no", "tile": ""}[cf.fill_option.lower()]
    options = f'input-ipc-server="{socket_path}" {cf.mpvpaper_options} loop {fill} background-color="{cf.color}"'
    if not cf.mpvpaper_sound:
        options += " no-audio"
    subprocess.Popen(["mpvpaper", "--fork", "--auto-pause", "--auto-mode", "MAX", "-o", options, "*" if monitor == "All" else monitor, str(image_path)])

'''
p.write_text(s[:start]+replacement+s[end:])
