#!/usr/bin/env python3
"""
Teensy MIDI Synth Monitor - Bidirectional Version
"""

import sys
import threading
import tkinter as tk
from tkinter import font as tkfont
import serial
import serial.tools.list_ports
import re

# ── Note name helper ────────────────────────────────────────────────────────
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

def midi_to_name(midi_note: int) -> str:
    if midi_note == -2:
        return "OFF"
    if midi_note < 0:
        return "—"
    octave = (midi_note // 12) - 1
    name   = NOTE_NAMES[midi_note % 12]
    return f"{name}{octave}"

# ── Serial reader (background thread) ───────────────────────────────────────
class SerialReader(threading.Thread):
    # Updated Regex to include D: (Distortion)
    PATTERN = re.compile(r"V:([\d.]+),N:(-?\d+),F:([\d.]+),G:([\d.]+),D:([\d.]+)")

    def __init__(self, port: str, baud: int, callback):
        super().__init__(daemon=True)
        self.port     = port
        self.baud     = baud
        self.callback = callback
        self._stop    = threading.Event()
        self.ser      = None # Accessible handle for writing

    def run(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
        except serial.SerialException as exc:
            print(f"[Serial] Could not open {self.port}: {exc}")
            return

        while not self._stop.is_set():
            try:
                line = self.ser.readline().decode("ascii", errors="ignore").strip()
                m = self.PATTERN.search(line)
                if m:
                    self.callback(
                        volume=float(m.group(1)),
                        note=int(m.group(2)),
                        freq=float(m.group(3)),
                        gain=float(m.group(4)),
                        dist=float(m.group(5))
                    )
            except Exception:
                pass
        if self.ser:
            self.ser.close()

    def stop(self):
        self._stop.set()

# ── GUI ──────────────────────────────────────────────────────────────────────
BG        = "#1a1a2e"
PANEL     = "#16213e"
ACCENT    = "#e94560"
TEXT      = "#eaeaea"
DIM       = "#6c7a89"
BAR_FULL  = "#e94560"
BAR_EMPTY = "#2a2a4a"

class SynthMonitor(tk.Tk):
    def __init__(self, port: str):
        super().__init__()
        self.title("Teensy MIDI Synth")
        self.resizable(False, False)
        self.configure(bg=BG)

        # fonts
        self.big   = tkfont.Font(family="Helvetica", size=48, weight="bold")
        self.med   = tkfont.Font(family="Helvetica", size=20, weight="bold")
        self.small = tkfont.Font(family="Helvetica", size=12)
        self.label_font = tkfont.Font(family="Helvetica", size=10)

        # State
        self._volume = 0.0
        self._note   = -1
        self._freq   = 0.0
        self._gain   = 0.0
        self._dist   = 0.0
        self._dirty  = False
        self._lock   = threading.Lock()

        self._build_ui()

        # Start serial reader
        self.reader = SerialReader(port, 115200, self._on_data)
        self.reader.start()

        # Poll for UI updates
        self.after(40, self._refresh_ui)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _on_dist_change(self, value):
        """Sends distortion value to Teensy"""
        if self.reader.ser and self.reader.ser.is_open:
            try:
                cmd = f"D:{float(value):.2f}\n"
                self.reader.ser.write(cmd.encode())
            except Exception as e:
                print(f"Error sending D to Teensy: {e}")

    def _on_wave_change(self):
        """Sends waveform type to Teensy: format 'W:1\n'"""
        if self.reader.ser and self.reader.ser.is_open:
            try:
                selection = self.wave_var.get()
                cmd = f"W:{selection}\n"
                self.reader.ser.write(cmd.encode())
            except Exception as e:
                print(f"Error sending W: {e}")

    def _build_ui(self):
        PAD = 24
        # Title bar
        title_frame = tk.Frame(self, bg=BG)
        title_frame.pack(fill="x", padx=PAD, pady=(PAD, 0))
        tk.Label(title_frame, text="🎹  TEENSY SYNTH", font=self.med,
                 bg=BG, fg=ACCENT).pack(side="left")
        self.status_dot = tk.Label(title_frame, text="●", font=self.med,
                                    bg=BG, fg=DIM)
        self.status_dot.pack(side="right")

        sep = tk.Frame(self, bg=ACCENT, height=2)
        sep.pack(fill="x", padx=PAD, pady=(8, PAD))

        # Main grid
        main = tk.Frame(self, bg=BG)
        main.pack(padx=PAD, pady=0)

        # NOTE panel
        note_panel = tk.Frame(main, bg=PANEL, bd=0)
        note_panel.grid(row=0, column=0, padx=(0, 16), sticky="nsew")

        tk.Label(note_panel, text="NOTE", font=self.label_font,
                 bg=PANEL, fg=DIM).pack(pady=(18, 0))
        self.note_lbl = tk.Label(note_panel, text="—", font=self.big,
                                 bg=PANEL, fg=TEXT, width=8)
        self.note_lbl.pack(padx=24)
        self.freq_lbl = tk.Label(note_panel, text="— Hz", font=self.small,
                                 bg=PANEL, fg=DIM)
        self.freq_lbl.pack(pady=(0, 18))

        # RIGHT panel
        right = tk.Frame(main, bg=BG)
        right.grid(row=0, column=1, sticky="nsew")

        # Volume bar
        vol_panel = tk.Frame(right, bg=PANEL)
        vol_panel.pack(fill="x", pady=(0, 12))
        vol_top = tk.Frame(vol_panel, bg=PANEL)
        vol_top.pack(fill="x", padx=16, pady=(14, 6))
        tk.Label(vol_top, text="VOLUME (pot A0)", font=self.label_font,
                 bg=PANEL, fg=DIM).pack(side="left")
        self.vol_pct = tk.Label(vol_top, text="0 %", font=self.label_font,
                                bg=PANEL, fg=TEXT)
        self.vol_pct.pack(side="right")
        bar_bg = tk.Frame(vol_panel, bg=BAR_EMPTY, height=18)
        bar_bg.pack(fill="x", padx=16, pady=(0, 14))
        bar_bg.pack_propagate(False)
        self.vol_bar = tk.Frame(bar_bg, bg=BAR_FULL, height=18)
        self.vol_bar.place(x=0, y=0, relheight=1.0, relwidth=0.0)

        # Velocity bar
        vel_panel = tk.Frame(right, bg=PANEL)
        vel_panel.pack(fill="x")
        vel_top = tk.Frame(vel_panel, bg=PANEL)
        vel_top.pack(fill="x", padx=16, pady=(14, 6))
        tk.Label(vel_top, text="VELOCITY (MIDI)", font=self.label_font,
                 bg=PANEL, fg=DIM).pack(side="left")
        self.vel_pct = tk.Label(vel_top, text="0 %", font=self.label_font,
                                bg=PANEL, fg=TEXT)
        self.vel_pct.pack(side="right")
        vel_bar_bg = tk.Frame(vel_panel, bg=BAR_EMPTY, height=18)
        vel_bar_bg.pack(fill="x", padx=16, pady=(0, 14))
        vel_bar_bg.pack_propagate(False)
        self.vel_bar = tk.Frame(vel_bar_bg, bg="#4ecdc4", height=18)
        self.vel_bar.place(x=0, y=0, relheight=1.0, relwidth=0.0)

        # DISTORTION SLIDER
        tk.Label(self, text="DISTORTION PRESET", fg=DIM, bg=BG, font=self.label_font).pack(pady=(20, 0))
        self.dist_slider = tk.Scale(self, from_=0, to=1, resolution=0.01,
                                     orient='horizontal', bg=BG, fg=ACCENT,
                                     troughcolor=DIM, highlightthickness=0,
                                     command=self._on_dist_change)
        self.dist_slider.pack(fill='x', padx=PAD, pady=(0, PAD))
        
        # WAVEFORM PICKER
        self.wave_var = tk.IntVar(value=0) # 0 = Sine by default
        wave_frame = tk.Frame(self, bg=BG)
        wave_frame.pack(pady=10)

        waveforms = [("SINE", 0), ("TRI", 1), ("SAW", 2), ("SQR", 3)]

        for text, value in waveforms:
            rb = tk.Radiobutton(wave_frame, text=text, variable=self.wave_var, value=value,
                                indicatoron=0, width=10, bg=PANEL, fg=DIM, 
                                selectcolor=ACCENT, activebackground=ACCENT,
                                command=self._on_wave_change)
            rb.pack(side="left", padx=5)
            
        # ATTACK SLIDER
        tk.Label(self, text="ATTACK (ms)", fg=DIM, bg=BG, font=self.label_font).pack(pady=(10, 0))
        self.attack_slider = tk.Scale(self, from_=1, to=2000, orient='horizontal', 
                                      bg=BG, fg=ACCENT, troughcolor=DIM, highlightthickness=0,
                                      command=lambda v: self._send_cmd("A", v))
        self.attack_slider.set(10) # Default
        self.attack_slider.pack(fill='x', padx=PAD)

        # RELEASE SLIDER
        tk.Label(self, text="RELEASE (ms)", fg=DIM, bg=BG, font=self.label_font).pack(pady=(10, 0))
        self.release_slider = tk.Scale(self, from_=1, to=2000, orient='horizontal', 
                                       bg=BG, fg=ACCENT, troughcolor=DIM, highlightthickness=0,
                                       command=lambda v: self._send_cmd("R", v))
        self.release_slider.set(500) # Default
        self.release_slider.pack(fill='x', padx=PAD, pady=(0, PAD))

    def _on_data(self, volume, note, freq, gain, dist):
        with self._lock:
            self._volume = volume
            self._note   = note
            self._freq   = freq
            self._gain   = gain
            self._dist   = dist
            self._dirty  = True

    def _refresh_ui(self):
        with self._lock:
            if not self._dirty:
                self.after(40, self._refresh_ui)
                return
            vol, note, freq, gain, dist = self._volume, self._note, self._freq, self._gain, self._dist
            self._dirty = False

        if note == -2: # POWER OFF
            self.note_lbl.config(text="OFF", fg="#e74c3c")
            self.status_dot.config(fg=DIM)
            self.vol_bar.place(relwidth=0)
            self.vel_bar.place(relwidth=0)
        else:
            self.status_dot.config(fg="#2ecc71")
            self.note_lbl.config(text=midi_to_name(note),
                                 fg=ACCENT if note >= 0 else DIM)
            self.freq_lbl.config(text=f"{freq:.1f} Hz" if note >= 0 else "— Hz")
            self.vol_pct.config(text=f"{int(vol * 100)} %")
            self.vol_bar.place(relwidth=min(vol, 1.0))
            self.vel_pct.config(text=f"{int(gain * 100)} %")
            self.vel_bar.place(relwidth=min(gain, 1.0))

        self.after(40, self._refresh_ui)

    def _on_close(self):
        self.reader.stop()
        self.destroy()
     
    def _send_cmd(self, tag, value):
        """Helper to send A:val or R:val to Teensy"""
        if self.reader.ser and self.reader.ser.is_open:
            try:
                cmd = f"{tag}:{float(value):.1f}\n"
                self.reader.ser.write(cmd.encode())
            except Exception as e:
                print(f"Error sending {tag}: {e}")

def pick_port(argv) -> str:
    if len(argv) > 1: return argv[1]
    ports = serial.tools.list_ports.comports()
    if not ports: sys.exit(1)
    if len(ports) == 1: return ports[0].device
    for i, p in enumerate(ports): print(f"  [{i}] {p.device}")
    return ports[int(input("Select port: "))].device

if __name__ == "__main__":
    port = pick_port(sys.argv)
    app = SynthMonitor(port)
    app.mainloop()
