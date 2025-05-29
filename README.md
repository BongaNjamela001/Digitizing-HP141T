# HP141T Spectrogram Display Project

This repository contains the complete set of files for the HP141T Spectrogram Display project, including hardware designs, simulations, software, and documentation. The project aims to visualize time-domain spectrogram data from the HP141T spectrum analyzer on a touchscreen LCD, replicating the CRT display with a green trace and grid.

## Repository Structure

The repository is organized into the following main folders:

- **PCBs/**: Hardware design files for the HP141T emulator and Signal Conditioning Subsystem.
- **Presentation/**: Materials for project presentations.
- **Report/**: Project report and associated LaTeX files.
- **Results/**: Output data and results from the project.
- **Simulations/**: Simulation files for circuit and signal analysis.
- **Software/**: Python scripts for data processing and GUI implementation.

### PCBs
This folder contains the PCB design files for the project, divided into two subdirectories:

- **HP141T_Emulator/**: KiCad project files and Gerber files for the HP141T emulator PCB.
- **Signal_Conditioning_Subsystem/**: KiCad project files and Gerber files for the Signal Conditioning Subsystem PCB.

### Presentation
This folder includes materials prepared for project presentations:

- **Poster/**: The project poster in PDF format.
- **Oral_Assessment/**: PowerPoint presentation (`.pptx`) for the oral assessment.

### Report
This folder contains the project report and its source files:

- **LaTeX/**: LaTeX source files for the project report.
- **Final_Report/**: Compiled PDF of the final project report.

### Results
This folder stores the output data and results generated during the project. It includes CSV files (e.g., `das-data_01.csv` to `das-data_64.csv`) with time and voltage measurements used for spectrogram visualization.

### Simulations
This folder contains simulation files for circuit and signal analysis:

- **LTSpice/**: LTSpice simulation files for circuit design validation.
- **MATLAB/**: MATLAB scripts and simulation files for signal processing and analysis.

### Software
This folder includes the software developed for the project:

- **DPS/**: Python scripts for data processing (e.g., `spectrogram_display_documented.py`).
- **GUIs/**: Python scripts for graphical user interfaces to interact with the spectrogram display.

## Usage

To explore the repository:

1. **Hardware**: Use KiCad to open the PCB design files in the `PCBs/` folder. Gerber files can be used for manufacturing.
2. **Simulations**: Run LTSpice simulations directly from the `.asc` files, or use MATLAB to execute the signal processing scripts.
3. **Software**: Navigate to the `Software/DPS/` folder to run the Python scripts. Ensure dependencies like `pandas`, `matplotlib`, and `tkinter` are installed. For example:
   ```bash
   python spectrogram_display_documented.py
   ```
4. **Documentation**: View the final report in `Report/Final_Report/` or compile the LaTeX source in `Report/LaTeX/`.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details (to be added).

## Contributing

Contributions are welcome! Please submit a pull request or open an issue to discuss improvements or bug fixes.