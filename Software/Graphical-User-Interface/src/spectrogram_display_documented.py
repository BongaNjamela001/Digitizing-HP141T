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
# Ensure the log directory exists, or change the path to a writable location
log_file_path = '/home/pi/spectrogram_display.log'
if not os.path.exists(os.path.dirname(log_file_path)):
    try:
        os.makedirs(os.path.dirname(log_file_path))
    except OSError as e:
        print(f"Warning: Could not create log directory {os.path.dirname(log_file_path)}: {e}. Logging to current directory.")
        log_file_path = 'spectrogram_display.log' # Fallback to current directory

logging.basicConfig(filename=log_file_path, level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s')

# Global variables
pause_flag = False
processor = SpectrumProcessor()
current_file_index = 1 # Start with das-data_01.csv, assuming files are das-data_01.csv to das-data_64.csv

# Global references for Matplotlib objects to allow updating without re-creating
fig = None
ax = None
line = None # This will hold the plot line object
canvas = None
root = None # Global reference to the Tkinter root window
status_label = None # Global reference for the status label

def pause_recording():
    """Pauses the data recording and plot updates."""
    global pause_flag
    pause_flag = True
    status_label.config(text="Recording Paused")
    logging.info("Recording paused by user")

def set_mode_phm():
    """Sets the display mode to Peak Hold Mode (PHM)."""
    processor.set_mode('PHM')
    status_label.config(text="Mode: Peak Hold")
    logging.info("Switched to Peak Hold Mode")

def set_mode_avm():
    """Sets the display mode to Average Mode (AvM)."""
    processor.set_mode('AvM')
    status_label.config(text="Mode: Average")
    logging.info("Switched to Average Mode")

def set_mode_rwm():
    """Sets the display mode to Raw Mode (RwM)."""
    processor.set_mode('RwM')
    status_label.config(text="Mode: Raw")
    logging.info("Switched to Raw Mode")

def show_help():
    """Display a tutorial window with information about the application."""
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
    - Data is loaded from 'das-data_X.csv' files in /home/pi/Documents/Digitizing-HP141T/GUIS/src.
    - Contact support for issues (log at /home/pi/spectrogram_display.log).

    Close this window to return to the display.
    """
    
    # Create and configure text widget
    text_widget = tk.Text(help_window, bg='black', fg='white', font=('Courier', 10), wrap='word')
    text_widget.insert(tk.END, tutorial_text)
    text_widget.config(state='disabled')  # Make read-only
    text_widget.pack(expand=True, fill='both', padx=10, pady=10)

def load_latest_csv():
    """
    Loads the next CSV file in sequence (das-data_01.csv to das-data_64.csv).
    Handles common parsing issues like commas as decimal separators and extra spaces.
    """
    global current_file_index
    try:
        # Define the directory where CSV files are expected
        csv_dir = '/home/pi/Documents/Digitizing-HP141T/GUIS/src'
        
        # Check if the directory exists
        if not os.path.exists(csv_dir):
            logging.error(f"CSV directory not found: {csv_dir}")
            return None

        # List files and filter for 'das-data_XX.csv'
        files = [f for f in os.listdir(csv_dir) if f.startswith('das-data_') and f.endswith('.csv')]
        if not files:
            logging.warning(f"No CSV files found in {csv_dir}")
            return None
        
        # Sort files numerically based on the number in their name
        files.sort(key=lambda x: int(x.split('_')[1].split('.')[0]))

        # Cycle through files from 1 to the number of available files
        if current_file_index > len(files):
            current_file_index = 1
        
        # Construct the full path to the current file
        latest_file = os.path.join(csv_dir, files[current_file_index - 1])
        
        # Read the CSV file using pandas
        # skiprows=3 to skip the first 3 lines (headers/metadata)
        # sep=';' to specify semicolon as the delimiter
        # names=['Time (ms)','Channel A (V)'] to assign column names
        df = pd.read_csv(latest_file, sep=';', skiprows=3, names=['Time (ms)','Channel A (V)'])
        logging.info(f"Loaded CSV file: {latest_file}")

        # Clean and convert 'Time (ms)' column to float
        df['Time (ms)'] = pd.to_numeric(
            df['Time (ms)'].astype(str).str.split().str[0].str.replace(',', '.'), 
            errors='coerce'
        )
        
        # Clean and convert 'Channel A (V)' column to float
        df['Channel A (V)'] = pd.to_numeric(
            df['Channel A (V)'].astype(str).str.split().str[0].str.replace(',', '.'), 
            errors='coerce'
        )

        # Drop rows where conversion resulted in NaN (missing or unparseable data)
        df.dropna(subset=['Time (ms)', 'Channel A (V)'], inplace=True)

        # Increment index for the next cycle
        current_file_index += 1
        return df
    except Exception as e:
        logging.error(f"Error loading or processing CSV: {str(e)}")
        return None

def update_plot():
    """
    Updates the spectrogram plot based on the current data and display mode.
    This function is called repeatedly to refresh the display.
    """
    global fig, ax, line, canvas # Declare global to modify them

    if not pause_flag:
        df = load_latest_csv()
        if df is not None and not df.empty: # Ensure DataFrame is not empty
            try:
                # Extract time in seconds and convert to milliseconds
                time_s = df['Time (ms)']
                time_ms = time_s * 1000  # Convert to milliseconds for consistency

                # Extract channel voltage (0 to 3.3 V) and scale to dB (0 to 80 dB)
                voltage = df['Channel A (V)']
                power_db = 80 * (3.3 - voltage) / 3.3

                # Process scan and get interpolated data from the SpectrumProcessor
                # processor.process_scan updates internal state based on mode
                processor.process_scan(time_ms.to_numpy(), power_db.to_numpy())
                
                # get_interpolated_plot_data uses the internal state to provide data for plotting
                time_smooth, power_smooth = processor.get_interpolated_plot_data()

                if time_smooth is not None and power_smooth is not None and len(time_smooth) > 0:
                    # Update data on the existing line object
                    line.set_xdata(time_smooth)
                    line.set_ydata(power_smooth)

                    # Update X-axis limits and ticks dynamically based on current data
                    time_max = time_smooth.max() if len(time_smooth) > 0 else 100
                    ax.set_xlim(0, time_max)
                    ax.set_xticks(np.linspace(0, time_max, 11))
                    ax.set_yticks(np.linspace(0, 80, 9)) # Y-ticks are fixed

                    # Redraw the canvas
                    canvas.draw_idle() # Use draw_idle for efficiency

                    logging.info(f"Time-domain plot updated in {processor.mode} mode")
                else:
                    logging.warning("Interpolated plot data is None or empty, skipping plot update.")
            except Exception as e:
                logging.error(f"Error during plot update: {str(e)}")
        else:
            logging.warning("DataFrame is None or empty, skipping plot update.")
    # Schedule the next update
    if root is not None: # Ensure root exists before scheduling
        root.after(1000, update_plot)

# Main application execution block
try:
    logging.info("Starting spectrogram display application at %s", 
                 datetime.now().strftime("%Y-%m-%d %H:%M:%S %Z"))

    # Check for required modules
    required_modules = ['matplotlib', 'tkinter', 'pandas', 'numpy', 'hp141t', 'scipy']
    for module in required_modules:
        try:
            __import__(module)
        except ImportError:
            logging.error(f"Missing required module: {module}. Please install it using 'pip3 install {module}'.")
            exit(1)

    # Initialize Tkinter root window
    root = tk.Tk()
    root.title("Spectrogram Display")
    root.geometry("800x480") # Set initial window size

    # Create a main frame to hold the plot and controls side-by-side
    main_content_frame = ttk.Frame(root)
    main_content_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    # Create Matplotlib figure and integrate into Tkinter
    # These are now global and initialized once
    fig = plt.figure(figsize=(8, 4.8)) # Initial size, will be managed by Tkinter packing
    ax = fig.add_subplot(111) # Get the axes object
    canvas = FigureCanvasTkAgg(fig, master=main_content_frame) # Master is main_content_frame
    canvas.get_tk_widget().pack(side=tk.LEFT, fill=tk.BOTH, expand=True) # Plot on the left, expands

    # Create a frame for buttons on the right
    control_frame = ttk.Frame(main_content_frame)
    control_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=5, pady=5) # Buttons on the right, fills vertically

    # Initialize the line object with empty data immediately
    # This ensures 'line' is always bound to a Line2D object from the start
    line, = ax.plot([], [], color='limegreen', linewidth=2) # Initialize with empty data

    # Set up initial plot aesthetics (background, grid, labels, limits)
    ax.set_facecolor('black')
    fig.patch.set_facecolor('black')
    ax.grid(True, color='gray', linestyle='--', linewidth=0.5)
    ax.set_xlabel('', color='white', fontsize=12)
    ax.set_ylabel('Amplitude (dB)', color='white', fontsize=12)
    ax.set_title('HP141T Display', color='white', fontsize=14)
    ax.tick_params(axis='both', colors='white')
    ax.set_ylim(0, 80) # Y-axis limit is fixed
    ax.set_xlim(0, 100) # Initial X-axis limit, will be updated dynamically

    # Mode selection buttons - packed into control_frame
    ttk.Button(control_frame, text="Peak Hold Mode", command=set_mode_phm).pack(side=tk.TOP, fill=tk.X, pady=5)
    ttk.Button(control_frame, text="Average Mode", command=set_mode_avm).pack(side=tk.TOP, fill=tk.X, pady=5)
    ttk.Button(control_frame, text="Raw Mode", command=set_mode_rwm).pack(side=tk.TOP, fill=tk.X, pady=5)
    ttk.Button(control_frame, text="Help", command=show_help).pack(side=tk.TOP, fill=tk.X, pady=5)

    pause_button = ttk.Button(control_frame, text="Pause Recording", command=pause_recording)
    pause_button.pack(side=tk.TOP, fill=tk.X, pady=5)

    # Status label at the very bottom of the root window
    status_label = ttk.Label(root, text="Mode: Raw", anchor=tk.CENTER)
    status_label.pack(side=tk.BOTTOM, fill=tk.X, pady=2)

    # Start the plot update loop
    update_plot()
    logging.info("Application started successfully")
    root.mainloop() # Start the Tkinter event loop

except Exception as e:
    logging.error(f"Application crashed: {str(e)}")
    exit(1)
