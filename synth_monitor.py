#!/usr/bin/env python3
"""
BC-26: ELECTRONIC KEYBOARD + RHYTHM MAPPED
Merged Version: Polyphonic Synth + Drum Machine Triggers
"""

import sys
import threading
import tkinter as tk
from tkinter import font as tkfont
import serial
import serial.tools.list_ports
import re

# -- Authentic 80s Hardware Palette --
CHASSIS_GRAY = "#C8C8B8"
LCD_SCREEN   = "#2D302D"
LED_ON       = "#FF3300"
LABEL_NAVY   = "#1A237E"
DECO_ORANGE  = "#FF8C00"
OFF_WHITE    = "#F0F0E0"
GREEN_ON     = "#00FF44"

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

def midi_to_name(midi_note: int) -> str:
    if midi_note == -2: return "OFF"
    if midi_note < 0: return "---"
    octave = (midi_note // 12) - 1
    return f"{NOTE_NAMES[midi_note % 12]}{octave}"

def find_teensy_port():
    ports = serial.tools.list_ports.comports()
    print("=== SEARCHING FOR HARDWARE ===")
    for p in ports:
        print(f"  DETECTED: {p.device} | {p.description} | {p.hwid}")
        
        # 1. Check for hardware ID (16C0 is Teensy)
        if "16C0" in p.hwid.upper():
            print(f"  >>> MATCH FOUND (Teensy HWID): {p.device}")
            return p.device
            
        # 2. Check for the specific name your Windows uses
        if "dispositivo serie" in p.description.lower():
            print(f"  >>> MATCH FOUND (Device Name): {p.device}")
            return p.device
            
        # 3. Standard English checks
        if "teensy" in p.description.lower() or "usb serial" in p.description.lower():
            print(f"  >>> MATCH FOUND (Legacy Check): {p.device}")
            return p.device
            
    print("=== NO AUTOMATIC MATCH FOUND ===")
    return None

class SerialReader(threading.Thread):
    # Regex matches the Teensy Serial.printf format: V, N, N2, N3, N4, G, D, B
    PATTERN = re.compile(r"V:([\d.]+),N:(-?\d+),N2:(-?\d+),N3:(-?\d+),N4:(-?\d+),G:([\d.]+),D:([\d.]+),B:([\d.]+)")

    def __init__(self, port, data_callback, status_callback):
        super().__init__(daemon=True)
        self.port = port
        self.data_callback = data_callback
        self.status_callback = status_callback
        self._stop = threading.Event()
        self.ser = None

    def run(self):
        if not self.port:
            self.status_callback(False, "NO DEVICE DETECTED")
            return

        try:
            # Adding dtr/rts to force the connection open
            self.ser = serial.Serial(self.port, 115200, timeout=0.1)
            self.ser.dtr = True
            self.ser.rts = True
            self.status_callback(True, self.port)
            print(f"--- CONNECTION ESTABLISHED ON {self.port} ---")
        except serial.SerialException as e:
            if "PermissionError" in str(e) or "Access is denied" in str(e):
                self.status_callback(False, "PORT BUSY (CLOSE ARDUINO MONITOR)")
            else:
                self.status_callback(False, "CONNECTION FAILED")
            print(f"ERROR: {e}")
            return

        while not self._stop.is_set():
            try:
                if self.ser.in_waiting > 0:
                    line = self.ser.readline().decode("ascii", errors="ignore").strip()
                    if not line: continue
                    m = self.PATTERN.search(line)
                    if m:
                        self.data_callback(
                            vol=float(m.group(1)),
                            notes=[int(m.group(2)), int(m.group(3)),
                                   int(m.group(4)), int(m.group(5))],
                            bend=float(m.group(8))
                        )
            except Exception as e:
                print(f"Runtime Error: {e}")
                break

    def send(self, data: bytes):
        if self.ser and self.ser.is_open:
            self.ser.write(data)

class SynthMonitor(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("BC-26 ELECTRONIC KEYBOARD - RHYTHM EDITION")
        self.configure(bg=CHASSIS_GRAY)
        self.resizable(False, False)

        # Fonts
        self.lcd_font    = tkfont.Font(family="Courier", size=32, weight="bold")
        self.label_font  = tkfont.Font(family="Arial Black", size=9)
        self.logo_font   = tkfont.Font(family="Arial Black", size=24, weight="bold", slant="italic")
        self.status_font = tkfont.Font(family="Courier", size=9)

        # State
        self.voice_notes = [-1, -1, -1, -1]
        self._volume = 0.0
        self._bend   = 1.0
        self._lock   = threading.Lock()
        self._dirty  = False
        
        port = find_teensy_port()
        self.is_offline = (port is None)

        self._build_ui()
        
        self.reader = SerialReader(port, self._on_data, self._on_status)
        self.reader.start()
        self._refresh_ui()

    def _build_ui(self):
        # Header / Branding
        header = tk.Frame(self, bg=CHASSIS_GRAY, pady=10)
        header.pack(fill="x")
        tk.Label(header, text="BC-26", font=self.logo_font, bg=CHASSIS_GRAY, fg=LABEL_NAVY).pack(side="left", padx=20)
        
        self.status_lbl = tk.Label(header, text="OFFLINE", font=self.status_font, bg=LCD_SCREEN, fg=LED_ON, width=25)
        self.status_lbl.pack(side="right", padx=20)

        # Faceplate
        faceplate = tk.Frame(self, bg=OFF_WHITE, bd=4, relief="sunken", padx=20, pady=20)
        faceplate.pack(padx=15, pady=15)

        # LCD Display for 4-Voice Polyphony
        display_frame = tk.Frame(faceplate, bg="#111", bd=8, relief="raised")
        display_frame.pack(fill="x", pady=(0, 20))
        self.note_labels = []
        for i in range(4):
            lbl = tk.Label(display_frame, text="---", font=self.lcd_font, bg=LCD_SCREEN, fg=LED_ON, width=4)
            lbl.pack(side="left", padx=4, pady=5)
            self.note_labels.append(lbl)

        # Main Controls Area
        ctrl_area = tk.Frame(faceplate, bg=OFF_WHITE)
        ctrl_area.pack(fill="x")

        # Column 1: Indicators (Vol Bar & Pitch Needle)
        col1 = tk.Frame(ctrl_area, bg=OFF_WHITE)
        col1.pack(side="left", padx=10, anchor="n")

        tk.Label(col1, text="VOLUME", font=self.label_font, bg=OFF_WHITE, fg=LABEL_NAVY).pack()
        self.vol_canvas = tk.Canvas(col1, width=120, height=15, bg=LCD_SCREEN, highlightthickness=0)
        self.vol_canvas.pack(pady=5)
        self.vol_bar = self.vol_canvas.create_rectangle(0, 0, 0, 15, fill=GREEN_ON)

        tk.Label(col1, text="PITCH BEND", font=self.label_font, bg=OFF_WHITE, fg=LABEL_NAVY).pack(pady=(10,0))
        self.bend_canvas = tk.Canvas(col1, width=120, height=30, bg=LCD_SCREEN, highlightthickness=0)
        self.bend_canvas.pack(pady=5)
        self.bend_canvas.create_line(60, 5, 60, 25, fill="white", dash=(2,2)) # Center
        self.bend_needle = self.bend_canvas.create_rectangle(58, 5, 62, 25, fill=LED_ON)

        # Column 2: Synth Parameters (Sliders)
        col2 = tk.Frame(ctrl_area, bg=OFF_WHITE)
        col2.pack(side="left", padx=20)

        params = [("DRIVE", "D", 0, 1.0), ("ATTACK", "A", 10, 1000), ("RELEASE", "R", 500, 2000)]
        for label, tag, start, top in params:
            f = tk.Frame(col2, bg=OFF_WHITE)
            f.pack(side="left", padx=5)
            tk.Label(f, text=label, font=self.label_font, bg=OFF_WHITE, fg=LABEL_NAVY).pack()
            s = tk.Scale(f, from_=top, to=0, orient="vertical", length=100, 
                         showvalue=0, bg=CHASSIS_GRAY, troughcolor=LCD_SCREEN,
                         command=lambda v, t=tag: self._send_cmd(t, v))
            s.set(start)
            s.pack()

        # Column 3: Rhythm & Voice (Buttons)
        col3 = tk.Frame(ctrl_area, bg=OFF_WHITE)
        col3.pack(side="left", padx=10, anchor="n")

        tk.Label(col3, text="RHYTHM", font=self.label_font, bg=OFF_WHITE, fg=LABEL_NAVY).pack()
        drum_f = tk.Frame(col3, bg=OFF_WHITE)
        drum_f.pack(pady=5)
        
        for d_name, d_cmd in [("KICK","kick"), ("SNARE","snare"), ("HHAT","hihat"), ("TOM","tom"), ("BELL","cowbell")]:
            btn = tk.Button(drum_f, text=d_name, width=7, font=("Arial", 7, "bold"),
                            bg=LABEL_NAVY, fg="white", activebackground=DECO_ORANGE,
                            command=lambda c=d_cmd: self._send_drum(c))
            btn.pack(pady=1)

        tk.Label(col3, text="WAVEFORM", font=self.label_font, bg=OFF_WHITE, fg=LABEL_NAVY).pack(pady=(10,0))
        self.wv_var = tk.IntVar(value=0)
        for i, name in enumerate(["SINE", "TRI", "SQR", "SAW"]):
            tk.Radiobutton(col3, text=name, variable=self.wv_var, value=i,
                           indicatoron=0, font=("Arial", 7, "bold"), width=8,
                           bg="#BBB", selectcolor=DECO_ORANGE,
                           command=lambda: self._send_cmd("W", self.wv_var.get())).pack(pady=1)

    def _send_cmd(self, tag, val):
        if self.is_offline: return
        msg = f"{tag}:{float(val)}\n".encode()
        self.reader.send(msg)

    def _send_drum(self, drum_type):
        if self.is_offline: return
        msg = f"DRUM:{drum_type}\n".encode()
        self.reader.send(msg)

    def _on_data(self, vol, notes, bend):
        with self._lock:
            self._volume = vol
            self.voice_notes = notes
            self._bend = bend
            self._dirty = True

    def _on_status(self, success, msg):
        color = GREEN_ON if success else LED_ON
        self.status_lbl.config(text=msg.upper(), fg=color)

    def _refresh_ui(self):
        if self._dirty:
            with self._lock:
                # Update Volume Bar
                vw = self.vol_canvas.winfo_width()
                self.vol_canvas.coords(self.vol_bar, 0, 0, vw * self._volume, 15)
                
                # Update Pitch Needle
                center = 60
                # Map bend (roughly 0.8 to 1.2) to pixel offset
                nx = max(5, min(115, center + (self._bend - 1.0) * 300))
                self.bend_canvas.coords(self.bend_needle, nx-2, 5, nx+2, 25)

                # Update LCD Note names
                for i in range(4):
                    self.note_labels[i].config(text=midi_to_name(self.voice_notes[i]))
                
                self._dirty = False
        
        self.after(50, self._refresh_ui)

if __name__ == "__main__":
    app = SynthMonitor()
    app.mainloop()
