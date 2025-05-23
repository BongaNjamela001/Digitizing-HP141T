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
from hp141t import SpectrumProcessor  # Import the HP141T library

# Set up logging
logging.basicConfig(filename='/home/pi/spectrogram_display.log', level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s')

# Global variables
pause_flag = False
processor = SpectrumProcessor()

def pause_recording():
    global pause_flag
    pause_flag = True
    status_label.config(text="Recording Paused")
    logging.info("Recording paused by user")

def set_mode_phm():
    processor.set_mode('PHM')
    status_label.config(text="Mode: Peak Hold")
    logging.info("Switched to Peak Hold Mode")

def set_mode_avm():
    processor.set_mode('AvM')
    status_label.config(text="Mode: Average")
    logging.info("Switched to Average Mode")

def set_mode_rwm():
    processor.set_mode('RwM')
    status_label.config(text="Mode: Raw")
    logging.info("Switched to Raw Mode")

def show_help():
    """Display a tutorial window"""
    help_window = tk.Toplevel(root)
    help_window.title("HELP - HP141T Spectrogram Tutorial")
    help_window.geometry("400x300")
    help_window.configure(bg='black')

    # Tutorial text
    tutorial_text = """
    === HP141T Spectrogram Tutorial ===

    Welcome to the HP141T Spectrogram Display!
    This system visualizes data from the HP141T spectrum analyzer on a touchscreen LCD.

    **Purpose:**
    - Displays time-domain spectrogram data (0 to 80 dB amplitude, time sweep).
    - Replicates the CRT display with a green trace and grid.

    **Modes:**
    - **Peak Hold Mode (PHM):** Shows the largest value seen at each time point, updated per scan.
    - **Average Mode (AvM):** Displays the running average of values at each time point, updated per scan.
    - **Raw Mode (RwM):** Shows the latest value, overwriting previous data during each scan.

    **Usage:**
    - **Mode Selection:** Tap 'Peak Hold Mode', 'Average Mode', or 'Raw Mode' to switch display modes.
    - **Pause:** Tap 'Pause Recording' to stop updates.
    - **Help:** Tap 'Help' to view this tutorial.
    - Navigate using touchscreen gestures if supported by future updates.

    **Notes:**
    - Data is loaded from 'spectrogram_data_X.csv' files in /home/pi.
    - Contact support for issues (log at /home/pi/spectrogram_display.log).

    Close this window to return to the display.
    """
    
    # Create and configure text widget
    text_widget = tk.Text(help_window, bg='black', fg='white', font=('Courier', 10), wrap='word')
    text_widget.insert(tk.END, tutorial_text)
    text_widget.config(state='disabled')  # Make read-only
    text_widget.pack(expand=True, fill='both', padx=10, pady=10)

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

                # Process scan and get interpolated data
                processor.process_scan(time_ms, power_db)
                time_smooth, power_smooth = processor.get_interpolated_plot_data()

                if time_smooth is not None and power_smooth is not None:
                    plt.clf()
                    ax = plt.gca()

                    # Plot with CRT styling
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
                    ax.set_xlabel('Time (ms)', color='white', fontsize=12)
                    ax.set_ylabel('Amplitude (dB)', color='white', fontsize=12)
                    ax.set_title('HP141T Spectrogram', color='white', fontsize=14)
                    ax.tick_params(axis='both', colors='white')

                    # Set axis limits
                    ax.set_xlim(0, time_max)
                    ax.set_ylim(0, 80)

                    canvas.draw()
                    logging.info(f"Time-domain plot updated in {processor.mode} mode")
            except Exception as e:
                logging.error(f"Error updating plot: {str(e)}")
    root.after(1000, update_plot)

# Main application
try:
    logging.info("Starting spectrogram display application at %s", 
                 datetime.now().strftime("%Y-%m-%d %H:%M:%S %Z"))

    # Check dependencies
    required_modules = ['matplotlib', 'tkinter', 'pandas', 'numpy', 'hp141t_library', 'scipy']
    for module in required_modules:
        try:
            __import__(module)
        except ImportError:
            logging.error(f"Missing required module: {module}")
            exit(1)

    root = tk.Tk()
    root.title("Spectrogram Display")
    root.geometry("800x480")

    fig = plt.figure(figsize=(8, 4.8))
    canvas = FigureCanvasTkAgg(fig, master=root)
    canvas.get_tk_widget().pack()

    # Mode selection buttons
    ttk.Button(root, text="Peak Hold Mode", command=set_mode_phm).pack()
    ttk.Button(root, text="Average Mode", command=set_mode_avm).pack()
    ttk.Button(root, text="Raw Mode", command=set_mode_rwm).pack()
    ttk.Button(root, text="Help", command=show_help).pack()  # New Help button

    pause_button = ttk.Button(root, text="Pause Recording", command=pause_recording)
    pause_button.pack()

    status_label = ttk.Label(root, text="Mode: Raw")
    status_label.pack()

    update_plot()
    logging.info("Application started successfully")
    root.mainloop()

except Exception as e:
    logging.error(f"Application crashed: {str(e)}")
    exit(1)
