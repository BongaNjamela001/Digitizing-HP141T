import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import tkinter as tk
from tkinter import ttk
import pandas as pd
import os
import time
import logging
from datetime import datetime

# Set up logging
logging.basicConfig(filename='/home/pi/spectrogram_display.log', level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s')

# Global pause flag
pause_flag = False

def pause_recording():
    global pause_flag
    pause_flag = True
    status_label.config(text="Recording Paused")
    logging.info("Recording paused by user")

def load_latest_csv():
    try:
        files = [f for f in os.listdir('/home/pi') if f.startswith('spectrogram_data_') and f.endswith('.csv')]
        if not files:
            logging.warning("No CSV files found in /home/pi")
            return None
        files.sort(key=lambda x: int(x.split('_')[2].split('.')[0]))
        latest_file = os.path.join('/home/pi', files[-1])
        df = pd.read_csv(latest_file)
        logging.info(f"Loaded CSV file: {latest_file}")
        return df
    except Exception as e:
        logging.error(f"Error loading CSV: {str(e)}")
        return None

def update_plot():
    if not pause_flag:
        df = load_latest_csv()
        if df is not None:
            try:
                plt.clf()
                plt.plot(df['Time (ms)'], df['V_y (V)'], label='V_y (V)')
                plt.grid(True)
                plt.title('Frequency Sweep Spectrogram')
                plt.xlabel('Time (ms)')
                plt.ylabel('Amplitude (V)')
                plt.ylim(-0.1, 3.4)
                canvas.draw()
                logging.info("Plot updated successfully")
            except Exception as e:
                logging.error(f"Error updating plot: {str(e)}")
    root.after(1000, update_plot)

# Main application
try:
    logging.info("Starting spectrogram display application")
    
    # Check dependencies
    required_modules = ['matplotlib', 'tkinter', 'pandas']
    for module in required_modules:
        if module not in globals():
            logging.error(f"Missing required module: {module}")
            exit(1)

    root = tk.Tk()
    root.title("Spectrogram Display")
    root.geometry("800x480")

    fig, ax = plt.subplots(figsize=(8, 4.8))
    canvas = FigureCanvasTkAgg(fig, master=root)
    canvas.get_tk_widget().pack()

    pause_button = ttk.Button(root, text="Pause Recording", command=pause_recording)
    pause_button.pack()

    status_label = ttk.Label(root, text="Recording Active")
    status_label.pack()

    update_plot()
    logging.info("Application started successfully")
    root.mainloop()

except Exception as e:
    logging.error(f"Application crashed: {str(e)}")
    exit(1)
