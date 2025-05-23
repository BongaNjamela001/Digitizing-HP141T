from scipy.interpolate import CubicSpline
import numpy as np

class SpectrumProcessor:
    def __init__(self):
        self.mode = 'RwM'  # Default mode
        self.time_data = None
        self.power_data = None
        self.peak_data = None
        self.avg_data = None
        self.raw_data = None
        self.scan_count = 0

    def set_mode(self, mode):
        """Set the display mode: PHM, AvM, or RwM"""
        if mode in ['PHM', 'AvM', 'RwM']:
            self.mode = mode
        else:
            raise ValueError("Mode must be PHM, AvM, or RwM")

    def process_scan(self, time_ms, power_db):
        """Process a new scan of data, updating mode-specific values"""
        self.time_data = time_ms
        self.scan_count += 1

        # Initialize or update data arrays
        if self.peak_data is None or len(self.peak_data) != len(power_db):
            self.peak_data = np.zeros_like(power_db)
            self.avg_data = np.zeros_like(power_db)
            self.raw_data = np.zeros_like(power_db)
        
        # Update mode-specific data
        if self.mode == 'PHM':
            np.maximum(self.peak_data, power_db, out=self.peak_data)
            return self.peak_data
        elif self.mode == 'AvM':
            if self.scan_count == 1:
                self.avg_data = power_db
            else:
                self.avg_data = (self.avg_data * (self.scan_count - 1) + power_db) / self.scan_count
            return self.avg_data
        else:  # RwM
            self.raw_data = power_db
            return self.raw_data

    def get_interpolated_plot_data(self):
        """Return interpolated data for plotting"""
        if self.time_data is None or len(self.time_data) == 0:
            return None, None
        cs = CubicSpline(self.time_data, self.process_scan(self.time_data, 80 * (3.3 - np.array(self.raw_data)) / 3.3))
        time_smooth = np.linspace(self.time_data[0], self.time_data[-1], 1000)
        power_smooth = cs(time_smooth)
        return time_smooth, power_smooth

    def reset(self):
        """Reset all mode data"""
        self.peak_data = None
        self.avg_data = None
        self.raw_data = None
        self.scan_count = 0
