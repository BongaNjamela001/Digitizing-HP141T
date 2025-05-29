from scipy.interpolate import CubicSpline
import numpy as np

class SpectrumProcessor:
    def __init__(self):
        self.mode = 'RwM'  # Default mode: Raw Mode
        self.time_data = None
        self.power_data = None # This will store the *last* raw power data
        self.peak_data = None # Stores peak hold values
        self.avg_data = None  # Stores average values
        self.raw_data = None  # Stores raw values for the current scan
        self.scan_count = 0   # Counter for average mode

    def set_mode(self, mode):
        """
        Set the display mode for the spectrogram.
        Valid modes are 'PHM' (Peak Hold Mode), 'AvM' (Average Mode), or 'RwM' (Raw Mode).
        """
        if mode in ['PHM', 'AvM', 'RwM']:
            self.mode = mode
            # Reset scan count when mode changes to ensure average calculation starts fresh
            self.scan_count = 0
            # Optionally, clear existing mode data if a mode switch should reset the display
            # self.peak_data = None
            # self.avg_data = None
            # self.raw_data = None
        else:
            raise ValueError("Mode must be PHM, AvM, or RwM")

    def process_scan(self, time_ms, power_db):
        """
        Process a new scan of data.
        This method updates the internal peak, average, and raw data arrays
        based on the current display mode.
        
        Args:
            time_ms (np.array): Array of time values in milliseconds for the current scan.
            power_db (np.array): Array of power values in dB for the current scan.
        """
        self.time_data = time_ms
        self.power_data = power_db # Store the raw power data for potential future use or debugging
        self.scan_count += 1

        # Initialize or resize data arrays if they are None or their length doesn't match
        # the current scan's data length. This ensures arrays are always correctly sized.
        if self.peak_data is None or len(self.peak_data) != len(power_db):
            self.peak_data = np.zeros_like(power_db)
            self.avg_data = np.zeros_like(power_db)
            self.raw_data = np.zeros_like(power_db)
        
        # Update mode-specific data based on the current mode
        if self.mode == 'PHM':
            # In Peak Hold Mode, update each point with the maximum value seen so far
            np.maximum(self.peak_data, power_db, out=self.peak_data)
            # No return here, as get_interpolated_plot_data will fetch from self.peak_data
        elif self.mode == 'AvM':
            # In Average Mode, calculate a running average
            if self.scan_count == 1:
                # For the first scan, the average is just the current data
                self.avg_data = power_db
            else:
                # For subsequent scans, update the running average
                self.avg_data = (self.avg_data * (self.scan_count - 1) + power_db) / self.scan_count
            # No return here
        else:  # 'RwM' (Raw Mode)
            # In Raw Mode, simply store the latest scan data
            self.raw_data = power_db
            # No return here

    def get_interpolated_plot_data(self):
        """
        Return interpolated data for plotting based on the current display mode.
        This method uses the data already processed and stored internally
        (peak_data, avg_data, or raw_data).
        """
        if self.time_data is None or len(self.time_data) == 0:
            # If no time data is available, return None for both time and power
            return None, None

        data_to_interpolate = None
        if self.mode == 'PHM':
            data_to_interpolate = self.peak_data
        elif self.mode == 'AvM':
            data_to_interpolate = self.avg_data
        else:  # 'RwM'
            data_to_interpolate = self.raw_data

        # Ensure that the data to interpolate is not None and has sufficient length
        if data_to_interpolate is None or len(data_to_interpolate) == 0:
            return None, None

        # Perform cubic spline interpolation
        # Ensure time_data and data_to_interpolate have the same length for CubicSpline
        if len(self.time_data) != len(data_to_interpolate):
            # This indicates an internal inconsistency, log or handle appropriately
            print(f"Warning: Time data length ({len(self.time_data)}) does not match processed data length ({len(data_to_interpolate)}) for interpolation.")
            return None, None

        cs = CubicSpline(self.time_data, data_to_interpolate)
        
        # Generate a smoother range of time values for interpolation
        time_smooth = np.linspace(self.time_data[0], self.time_data[-1], 1000)
        
        # Get the interpolated power values
        power_smooth = cs(time_smooth)
        
        return time_smooth, power_smooth

    def reset(self):
        """
        Reset all internal mode data and scan count.
        This is useful if you want to start fresh with data accumulation.
        """
        self.peak_data = None
        self.avg_data = None
        self.raw_data = None
        self.scan_count = 0
#from scipy.interpolate import CubicSpline
#import numpy as np

#class SpectrumProcessor:
    #def __init__(self):
        #self.mode = 'RwM'  # Default mode
        #self.time_data = None
        #self.power_data = None
        #self.peak_data = None
        #self.avg_data = None
        #self.raw_data = None
        #self.scan_count = 0

    #def set_mode(self, mode):
        #"""Set the display mode: PHM, AvM, or RwM"""
        #if mode in ['PHM', 'AvM', 'RwM']:
            #self.mode = mode
        #else:
            #raise ValueError("Mode must be PHM, AvM, or RwM")

    #def process_scan(self, time_ms, power_db):
        #"""Process a new scan of data, updating mode-specific values"""
        #self.time_data = time_ms
        #self.scan_count += 1

        ## Initialize or update data arrays
        #if self.peak_data is None or len(self.peak_data) != len(power_db):
            #self.peak_data = np.zeros_like(power_db)
            #self.avg_data = np.zeros_like(power_db)
            #self.raw_data = np.zeros_like(power_db)
        
        ## Update mode-specific data
        #if self.mode == 'PHM':
            #np.maximum(self.peak_data, power_db, out=self.peak_data)
            #return self.peak_data
        #elif self.mode == 'AvM':
            #if self.scan_count == 1:
                #self.avg_data = power_db
            #else:
                #self.avg_data = (self.avg_data * (self.scan_count - 1) + power_db) / self.scan_count
            #return self.avg_data
        #else:  # RwM
            #self.raw_data = power_db
            #return self.raw_data

    #def get_interpolated_plot_data(self):
        #"""Return interpolated data for plotting"""
        #if self.time_data is None or len(self.time_data) == 0:
            #return None, None
        #cs = CubicSpline(self.time_data, self.process_scan(self.time_data, 80 * (3.3 - np.array(self.raw_data)) / 3.3))
        #time_smooth = np.linspace(self.time_data[0], self.time_data[-1], 1000)
        #power_smooth = cs(time_smooth)
        #return time_smooth, power_smooth

    #def reset(self):
        #"""Reset all mode data"""
        #self.peak_data = None
        #self.avg_data = None
        #self.raw_data = None
        #self.scan_count = 0
