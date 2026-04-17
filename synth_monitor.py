#!/usr/bin/env python3
import sys
import threading
import tkinter as tk
from tkinter import font as tkfont, ttk
import serial
import serial.tools.list_ports
import re
import time

# -- Authentic Casiotone "Cream & Navy" Palette --
CASIO_CREAM  = "#F5F5DC"  
CASIO_NAVY   = "#1A237E"  
CASIO_RED    = "#D32F2F"  
CASIO_BLACK  = "#212121"  
LCD_BG       = "#879787"  
LCD_TEXT     = "#222222"  
LED_SIGNAL   = "#00FF44"  

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

def midi_to_name(n, vol):
    """Converts MIDI note to string. Clears LCD if volume is near zero."""
    if n < 0 or vol < 0.005: return "---"
    return f"{NOTE_NAMES[n % 12]}{(n // 12) - 1}"

def find_teensy_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        d = p.description.lower()
        if any(x in d for x in ["teensy", "usb serial", "dispositivo"]):
            return p.device
    return None

class Metronome(threading.Thread):
    def __init__(self, send_cb):
        super().__init__(daemon=True)
        self.send_cb = send_cb
        self.bpm, self.num, self.den = 120, 4, 4
        self.enabled = False
        self.mode = "Standard"
        self._stop = threading.Event()

    def run(self):
        beat = 0
        while not self._stop.is_set():
            if self.enabled:
                interval = 60.0 / self.bpm * (4.0 / self.den)
                if self.mode == "Standard":
                    cmd = "DRUM:m_h\n" if beat % self.num == 0 else "DRUM:m_l\n"
                    self.send_cb(cmd.encode())
                else:
                    self.send_cb(b"DRUM:hihat\n")
                    if beat % self.num == 0: self.send_cb(b"DRUM:kick\n")
                    if beat % self.num == (self.num // 2): self.send_cb(b"DRUM:snare\n")
                
                beat = (beat + 1) % self.num
                time.sleep(interval)
            else:
                beat = 0
                time.sleep(0.1)
class SynthMonitor(tk.Tk):
    def __init__(self, port):
        super().__init__()
        self.title("BC-26 SYSTEM MONITOR PRO")
        self.configure(bg=CASIO_CREAM)
        
        # Internal State
        self.notes, self.vol, self.bend, self.dirty = [-1]*4, 0.0, 1.0, False
        self.lock = threading.Lock()
        
        try:
            # High frequency polling for smooth meters
            self.ser = serial.Serial(port, 115200, timeout=0.001) 
            self.ser.dtr = self.ser.rts = True
        except:
            self.ser = None

        self.metro = Metronome(self._raw_send)
        self._build_ui()
        self.metro.start()
        self._poll_serial()
        self._refresh()

    def _build_ui(self):
        main = tk.Frame(self, bg=CASIO_CREAM, padx=20, pady=20)
        main.pack(expand=True, fill="both")

        # Branding
        tk.Label(main, text="BC-26 ELECTRONIC KEYBOARD SYSTEM", font=("Arial Black", 14), 
                 bg=CASIO_CREAM, fg=CASIO_NAVY).pack(anchor="w")

        # LCD Display - Clear Greenish 80s Look
        lcd_border = tk.Frame(main, bg=CASIO_BLACK, bd=4, relief="sunken")
        lcd_border.pack(fill="x", pady=15)
        lcd_main = tk.Frame(lcd_border, bg=LCD_BG, padx=10, pady=10)
        lcd_main.pack(fill="x")
        
        self.lbls = []
        for i in range(4):
            v_f = tk.Frame(lcd_main, bg=LCD_BG)
            v_f.pack(side="left", expand=True)
            tk.Label(v_f, text=f"VOICE {i+1}", font=("Arial", 7, "bold"), bg=LCD_BG, fg=LCD_TEXT).pack()
            l = tk.Label(v_f, text="---", font=("Courier", 32, "bold"), bg=LCD_BG, fg=LCD_TEXT, width=4)
            l.pack(); self.lbls.append(l)

        # Control Panel Grid
        cp = tk.Frame(main, bg=CASIO_CREAM)
        cp.pack(fill="both")

        # 1. METRONOME (FULL CONTROLS Restored)
        m_f = tk.LabelFrame(cp, text=" RHYTHM BOX ", bg=CASIO_CREAM, fg=CASIO_NAVY, font=("Arial", 9, "bold"), padx=10)
        m_f.pack(side="left", fill="y", padx=5)
        
        self.bpm_s = tk.Scale(m_f, from_=240, to=40, orient="vertical", length=160, bg=CASIO_CREAM, command=self._upd_metro)
        self.bpm_s.set(120); self.bpm_s.pack(side="left")
        
        m_ctrls = tk.Frame(m_f, bg=CASIO_CREAM)
        m_ctrls.pack(side="left", padx=5)
        
        tk.Label(m_ctrls, text="MODE", bg=CASIO_CREAM, font=("Arial", 7, "bold")).pack()
        self.mode_c = ttk.Combobox(m_ctrls, values=["Standard", "Drum Beat"], state="readonly", width=10)
        self.mode_c.set("Standard"); self.mode_c.bind("<<ComboboxSelected>>", self._upd_metro); self.mode_c.pack()

        tk.Label(m_ctrls, text="MEASURE", bg=CASIO_CREAM, font=("Arial", 7, "bold")).pack(pady=(5,0))
        self.num_s = tk.Spinbox(m_ctrls, from_=1, to=16, width=5, command=self._upd_metro)
        self.num_s.delete(0,"end"); self.num_s.insert(0,"4"); self.num_s.pack()

        tk.Label(m_ctrls, text="BEAT DIV", bg=CASIO_CREAM, font=("Arial", 7, "bold")).pack(pady=(5,0))
        self.den_c = ttk.Combobox(m_ctrls, values=["2", "4", "8", "16"], width=5, state="readonly")
        self.den_c.set("4"); self.den_c.bind("<<ComboboxSelected>>", self._upd_metro); self.den_c.pack()

        self.m_btn = tk.Button(m_ctrls, text="START / STOP", bg=CASIO_RED, fg="white", font=("Arial", 8, "bold"), height=2, command=self._toggle_metro)
        self.m_btn.pack(fill="x", pady=10)
        # 2. METERS & SYNTH PARAMS (Center)
        c_f = tk.Frame(cp, bg=CASIO_CREAM)
        c_f.pack(side="left", padx=15)
        
        tk.Label(c_f, text="MASTER LEVEL", font=("Arial", 7, "bold"), bg=CASIO_CREAM).pack()
        self.v_can = tk.Canvas(c_f, width=160, height=14, bg="#CCC", highlightthickness=1)
        self.v_bar = self.v_can.create_rectangle(0, 0, 0, 14, fill=LED_SIGNAL, outline="")
        self.v_can.pack(pady=5)

        tk.Label(c_f, text="PITCH OFFSET", font=("Arial", 7, "bold"), bg=CASIO_CREAM).pack()
        self.b_can = tk.Canvas(c_f, width=160, height=24, bg=LCD_BG, highlightthickness=1)
        self.b_ndl = self.b_can.create_rectangle(78, 2, 82, 22, fill=CASIO_RED, outline="")
        self.b_can.pack()

        sl_f = tk.Frame(c_f, bg=CASIO_CREAM); sl_f.pack(pady=10)
        for t, n, mx in [("D","DRIVE",1.0), ("A","ATTACK",2000), ("R","RELEASE",2000)]:
            f = tk.Frame(sl_f, bg=CASIO_CREAM); f.pack(side="left", padx=8)
            tk.Scale(f, from_=mx, to=0, resolution=0.01 if mx==1.0 else 1, orient="vertical", 
                     length=100, bg=CASIO_CREAM, showvalue=False, command=lambda v, x=t: self._send(x, v)).pack()
            tk.Label(f, text=n, font=("Arial", 6, "bold"), bg=CASIO_CREAM).pack()

        # 3. WAVEFORMS & ALL PERCUSSION PADS (Right)
        r_f = tk.Frame(cp, bg=CASIO_CREAM); r_f.pack(side="left", padx=10)
        tk.Label(r_f, text="OSC WAVE", font=("Arial", 7, "bold"), bg=CASIO_CREAM, fg=CASIO_NAVY).pack(pady=(0,5))
        self.wv = tk.IntVar(value=0)
        for i, n in enumerate(["SINE", "TRI", "SQR", "SAW"]):
            tk.Radiobutton(r_f, text=n, variable=self.wv, value=i, indicatoron=0, width=10, 
                           bg="#D1D1BC", selectcolor=CASIO_RED, command=lambda: self._send("W", self.wv.get())).pack(pady=1)
        
        tk.Label(r_f, text="DRUM PADS", font=("Arial", 7, "bold"), bg=CASIO_CREAM, fg=CASIO_NAVY).pack(pady=(15,0))
        for d in ["kick", "snare", "hihat", "tom", "cowbell"]:
            tk.Button(r_f, text=d.upper(), font=("Arial", 7, "bold"), bg="#BBB", height=1,
                      command=lambda x=d: self._raw_send(f"DRUM:{x}\n".encode())).pack(fill="x", pady=1)

    def _upd_metro(self, _=None):
        try: 
            self.metro.bpm = int(self.bpm_s.get())
            self.metro.num = int(self.num_s.get())
            self.metro.den = int(self.den_c.get())
            self.metro.mode = self.mode_c.get()
        except: pass

    def _toggle_metro(self):
        self.metro.enabled = not self.metro.enabled
        self.m_btn.config(bg=CASIO_NAVY if self.metro.enabled else CASIO_RED)

    def _on_data(self, v, ns, b):
        with self.lock: self.vol, self.notes, self.bend, self.dirty = v, ns, b, True

    def _raw_send(self, b):
        if self.ser and self.ser.is_open:
            try: self.ser.write(b)
            except: pass

    def _send(self, t, v): self._raw_send(f"{t}:{float(v)}\n".encode())

    def _poll_serial(self):
        if self.ser and self.ser.is_open:
            try:
                while self.ser.in_waiting:
                    line = self.ser.readline().decode("ascii", errors="ignore").strip()
                    # REGEX: Matches the V:%.3f,N:%d,N2:%d,N3:%d,N4:%d,G:0.800,D:%.3f,B:%.3f format
                    m = re.search(r"V:([\d.]+),N:(-?\d+),N2:(-?\d+),N3:(-?\d+),N4:(-?\d+),G:[\d.]+,D:([\d.]+),B:([\d.]+)", line)
                    if m:
                        self._on_data(float(m.group(1)), 
                                      [int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5))], 
                                      float(m.group(8)))
            except: pass
        self.after(10, self._poll_serial)

    def _refresh(self):
        with self.lock:
            if self.dirty:
                # Update Volume Bar
                self.v_can.coords(self.v_bar, 0, 0, 160 * min(self.vol * 2.0, 1.0), 14)
                # Update Pitch Bend Needle (Center is 80)
                bx = 80 + (self.bend - 1.0) * 160
                self.b_can.coords(self.b_ndl, bx-2, 2, bx+2, 22)
                # Update LCD Notes with silence check to prevent "stuck" notes
                for i in range(4): 
                    self.lbls[i].config(text=midi_to_name(self.notes[i], self.vol))
                self.dirty = False
        self.after(30, self._refresh)

if __name__ == "__main__":
    port = find_teensy_port()
    app = SynthMonitor(port if port else (sys.argv[1] if len(sys.argv) > 1 else None))
    app.mainloop()
