#!/bin/bash

# Check if the correct number of arguments are given
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <binary> <destination-folder>"
    exit 1
fi

BINARY="$1"
DEST_FOLDER="$2"

# Get the directory of the binary
BINARY_DIR=$(dirname "$BINARY")

# Create the destination folder if it doesn't exist
mkdir -p "$DEST_FOLDER"

# Function to check if a DLL path is in a system folder
is_system_dll() {
    local path="$1"
    if [[ "$path" =~ ^C:\\WINDOWS\\SYSTEM32\\ ]]; then
        return 0
    else
        return 1
    fi
}

# Run ntldd on the binary and parse the output
ntldd "$BINARY" | while read -r line; do
    # Extract the path to the DLL
    DLL_PATH=$(echo "$line" | grep -oP '=> \K.*(?= \()')
    
    # If the DLL path is empty, get the DLL name from the line
    if [ -z "$DLL_PATH" ]; then
        DLL_NAME=$(echo "$line" | awk '{print $1}')
        DLL_PATH="$BINARY_DIR/$DLL_NAME"
    fi
    
    # Check if the DLL path is in a system folder
    if is_system_dll "$DLL_PATH"; then
        echo "Skipping system DLL: $DLL_PATH"
        continue
    fi

    # Check if the DLL path is not empty and if the file exists
    if [ -n "$DLL_PATH" ] && [ -f "$DLL_PATH" ]; then
        echo "Copying $DLL_PATH to $DEST_FOLDER"
        cp "$DLL_PATH" "$DEST_FOLDER"
    fi
done

echo "All necessary DLLs have been copied to $DEST_FOLDER"
