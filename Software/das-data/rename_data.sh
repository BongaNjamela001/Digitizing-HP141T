#!/bin/bash

# Define the directory where your CSV files are located
CSV_DIR="/home/bonga/Documents/EEE4022F/Digitizing-HP141T/Software/das-data"

echo "Starting process to make CSVs editable and rename them..."
echo "Target directory: $CSV_DIR"

# Check if the directory exists
if [ ! -d "$CSV_DIR" ]; then
    echo "Error: Directory '$CSV_DIR' not found."
    exit 1
fi

# Navigate to the CSV directory
cd "$CSV_DIR" || { echo "Error: Could not change to directory '$CSV_DIR'"; exit 1; }

# Loop through file indices from 1 to 64
for i in $(seq -w 1 64); do
    # Construct the original filename
    ORIGINAL_FILE="das-data_${i}.csv"

    # Check if the original file exists
    if [ -f "$ORIGINAL_FILE" ]; then
        echo "Processing $ORIGINAL_FILE..."

        # 1. Make the file writable
        # +w adds write permission for the owner
        chmod +w "$ORIGINAL_FILE"
        if [ $? -eq 0 ]; then
            echo "  - Permissions changed to writable for $ORIGINAL_FILE."
        else
            echo "  - Warning: Could not change permissions for $ORIGINAL_FILE. Skipping rename."
            continue # Skip to the next file if permissions couldn't be changed
        fi

        # 2. Calculate the new index (add 64 to the current index)
        # Use 'printf %02d' to ensure two-digit formatting for the new index
        NEW_INDEX=$((10#$i + 64)) # 10#$i forces base 10 interpretation for '01', '02' etc.
        NEW_FILE=$(printf "das-data_%02d.csv" "$NEW_INDEX")

        # 3. Rename the file
        mv "$ORIGINAL_FILE" "$NEW_FILE"
        if [ $? -eq 0 ]; then
            echo "  - Renamed $ORIGINAL_FILE to $NEW_FILE."
        else
            echo "  - Error: Could not rename $ORIGINAL_FILE to $NEW_FILE."
        fi
    else
        echo "  - Warning: File $ORIGINAL_FILE not found. Skipping."
    fi
done

echo "Process complete."
echo "Please verify the files in $CSV_DIR"
