```python
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import tkinter as tk
from tkinter import ttk
import pandas as pd
import os
import time

# Global pause flag
pause_flag = False

def pause_recording():
    global pause_flag
    pause_flag = True
    status_label.config(text="Recording Paused")

def load_latest_csv():
    files = [f for f in os.listdir('.') if f.startswith('spectrogram_data_') and f.endswith('.csv')]
    if not files:
        return None
    files.sort(key=lambda x: int(x.split('_')[1].split('.')[0]))
    latest_file = files[-1]
    return pd.read_csv(latest_file)

def update_plot():
    if not pause_flag:
        df = load_latest_csv()
        if df is not None:
            # Assume V_y (column 2) is the signal for frequency sweep
            plt.clf()
            plt.plot(df['Time (ms)'], df['V_y (V)'], label='V_y (V)')
            plt.grid(True)
            plt.title('Frequency Sweep Spectrogram')
            plt.xlabel('Time (ms)')
            plt.ylabel('Amplitude (V)')
            plt.ylim(-0.1, 3.4)  # Match 0-3.3 V range
            canvas.draw()
    root.after(1000, update_plot)  # Update every 1 second

# GUI Setup
root = tk.Tk()
root.title("Spectrogram Display")
root.geometry("800x480")  # Suitable for 7-inch touchscreen

fig, ax = plt.subplots(figsize=(8, 4.8))
canvas = FigureCanvasTkAgg(fig, master=root)
canvas.get_tk_widget().pack()

# Pause Button
pause_button = ttk.Button(root, text="Pause Recording", command=pause_recording)
pause_button.pack()

# Status Label
status_label = ttk.Label(root, text="Recording Active")
status_label.pack()

# Start the plot update loop
update_plot()

root.mainloop()
```