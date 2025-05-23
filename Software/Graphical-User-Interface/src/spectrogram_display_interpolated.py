import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import tkinter as tk
from tkinter import ttk
import pandas as pd
import numpy as np
import os
import time
import logging
from datetime import datetime
from scipy.interpolate import CubicSpline  # Added for spline interpolation

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
                # Scale V_y (0 to 3.3 V) to dB (0 to 80 dB)
                vy = df['V_y (V)'].values
                power_db = 80 * (3.3 - vy) / 3.3

                # Use time from CSV for x-axis
                time_ms = df['Time (ms)'].values

                # Perform cubic spline interpolation
                cs = CubicSpline(time_ms, power_db)
                time_smooth = np.linspace(time_ms[0], time_ms[-1], 1000)  # Increase resolution
                power_smooth = cs(time_smooth)

                plt.clf()
                ax = plt.gca()

                # Plot with CRT styling using spline-interpolated data
                ax.plot(time_smooth, power_smooth, color='limegreen', linewidth=2)

                # CRT-like background and grid
                ax.set_facecolor('black')
                fig.patch.set_facecolor('black')
                ax.grid(True, color='gray', linestyle='--', linewidth=0.5)

                # Grid divisions: 10 (time), 8 (amplitude)
                time_max = time_ms[-1]
                ax.set_xticks(np.linspace(0, time_max, 11))
                ax.set_yticks(np.linspace(0, 80, 9))

                # Labels and title
                ax.set_xlabel('Frequency (GHz)', color='white', fontsize=12)
                ax.set_ylabel('Amplitude (dB)', color='white', fontsize=12)
                ax.set_title('HP141T Spectrogram', color='white', fontsize=14)
                ax.tick_params(axis='both', colors='white')

                # Set axis limits
                ax.set_xlim(0, time_max)
                ax.set_ylim(0, 80)

                canvas.draw()
                logging.info("Frequency-domain plot updated successfully with spline interpolation")
            except Exception as e:
                logging.error(f"Error updating plot: {str(e)}")
    root.after(1000, update_plot)

# Main application
try:
    logging.info("Starting spectrogram display application at %s", 
                 datetime.now().strftime("%Y-%m-%d %H:%M:%S %Z"))

    # Check dependencies
    required_modules = ['matplotlib', 'tkinter', 'pandas', 'numpy', 'scipy']
    for module in required_modules:
        if module not in globals():
            logging.error(f"Missing required module: {module}")
            exit(1)

    root = tk.Tk()
    root.title("Spectrogram Display")
    root.geometry("800x480")

    fig = plt.figure(figsize=(8, 4.8))
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
