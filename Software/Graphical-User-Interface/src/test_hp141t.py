import unittest
import pandas as pd
import numpy as np
import os
from hp141t_library import SpectrumProcessor
import tkinter as tk
from spectrogram_display import load_latest_csv, update_plot, processor, pause_flag

class TestHP141TSystem(unittest.TestCase):
    def setUp(self):
        """Set up test environment with mock CSV data"""
        # Create a mock CSV file
        self.csv_path = '/home/pi/test_spectrogram_data.csv'
        time_ms = np.linspace(0, 0.069444, 500)  # 500 points at 7.2 MSPS
        v_y = np.linspace(0, 3.3, 500)  # Linear ramp from 0 to 3.3 V
        v_x = np.linspace(0, 3.3, 500)  # Simulated scan input
        v_z = np.zeros(500)  # Ignored for now
        df = pd.DataFrame({
            'Time (ms)': time_ms,
            'V_x (V)': v_x,
            'V_y (V)': v_y,
            'V_z (V)': v_z
        })
        df.to_csv(self.csv_path, index=False)
        
        # Reset processor state
        processor.reset()
        global pause_flag
        pause_flag = False

    def tearDown(self):
        """Clean up test environment"""
        if os.path.exists(self.csv_path):
            os.remove(self.csv_path)

    def test_data_loading(self):
        """Test HP8555A and HP8552B data loading and scaling"""
        df = load_latest_csv()
        self.assertIsNotNone(df, "CSV loading failed")
        self.assertEqual(len(df), 500, "Incorrect number of data points")
        
        # Check V_y scaling (HP8555A RF signal)
        vy = df['V_y (V)'].values
        power_db = 80 * (3.3 - vy) / 3.3
        self.assertAlmostEqual(power_db[0], 80.0, places=2, msg="V_y scaling max incorrect")
        self.assertAlmostEqual(power_db[-1], 0.0, places=2, msg="V_y scaling min incorrect")
        
        # Check time scaling (HP8552B IF sweep)
        time_ms = df['Time (ms)'].values
        self.assertAlmostEqual(time_ms[-1], 0.069444, places=5, msg="Time sweep incorrect")

    def test_phm_mode(self):
        """Test Peak Hold Mode (PHM)"""
        processor.set_mode('PHM')
        df = load_latest_csv()
        time_ms = df['Time (ms)'].values
        power_db = 80 * (3.3 - df['V_y (V)'].values) / 3.3
        
        # First scan
        processed = processor.process_scan(time_ms, power_db)
        self.assertTrue(np.all(processed == power_db), "PHM first scan incorrect")
        
        # Second scan with lower values
        power_db_lower = power_db - 10  # Simulate a weaker signal
        processed = processor.process_scan(time_ms, power_db_lower)
        self.assertTrue(np.all(processed == power_db), "PHM did not hold peak values")

    def test_avm_mode(self):
        """Test Average Mode (AvM)"""
        processor.set_mode('AvM')
        df = load_latest_csv()
        time_ms = df['Time (ms)'].values
        power_db = 80 * (3.3 - df['V_y (V)'].values) / 3.3
        
        # First scan
        processed = processor.process_scan(time_ms, power_db)
        self.assertTrue(np.all(processed == power_db), "AvM first scan incorrect")
        
        # Second scan with different values
        power_db_alt = power_db - 20  # Simulate a different signal
        processed = processor.process_scan(time_ms, power_db_alt)
        expected_avg = (power_db + power_db_alt) / 2
        self.assertTrue(np.allclose(processed, expected_avg, rtol=1e-5), "AvM averaging incorrect")

    def test_rwm_mode(self):
        """Test Raw Mode (RwM)"""
        processor.set_mode('RwM')
        df = load_latest_csv()
        time_ms = df['Time (ms)'].values
        power_db = 80 * (3.3 - df['V_y (V)'].values) / 3.3
        
        # First scan
        processed = processor.process_scan(time_ms, power_db)
        self.assertTrue(np.all(processed == power_db), "RwM first scan incorrect")
        
        # Second scan overwrites
        power_db_alt = power_db + 10
        processed = processor.process_scan(time_ms, power_db_alt)
        self.assertTrue(np.all(processed == power_db_alt), "RwM did not overwrite values")

    def test_display_grid(self):
        """Test HP141T display grid and styling (mocked plot)"""
        df = load_latest_csv()
        time_ms = df['Time (ms)'].values
        power_db = 80 * (3.3 - df['V_y (V)'].values) / 3.3
        processor.process_scan(time_ms, power_db)
        time_smooth, power_smooth = processor.get_interpolated_plot_data()
        
        self.assertEqual(len(time_smooth), 1000, "Interpolated time points incorrect")
        self.assertEqual(len(power_smooth), 1000, "Interpolated power points incorrect")
        self.assertAlmostEqual(time_smooth[-1], time_ms[-1], places=5, msg="Time range incorrect")
        self.assertTrue(power_smooth.min() >= 0 and power_smooth.max() <= 80, "Power range incorrect")

    def test_button_functions(self):
        """Test button functionality"""
        # Mock button clicks
        set_mode_phm()
        self.assertEqual(processor.mode, 'PHM', "PHM button failed")
        
        set_mode_avm()
        self.assertEqual(processor.mode, 'AvM', "AvM button failed")
        
        set_mode_rwm()
        self.assertEqual(processor.mode, 'RwM', "RwM button failed")
        
        global pause_flag
        pause_recording()
        self.assertTrue(pause_flag, "Pause button failed")

if __name__ == '__main__':
    unittest.main()
