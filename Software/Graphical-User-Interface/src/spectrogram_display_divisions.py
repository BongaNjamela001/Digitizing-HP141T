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
    if not pause_flag
