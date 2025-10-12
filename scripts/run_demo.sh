#!/usr/bin/bash

# Define constants
PYTHON_FILE="../user/cli/main.py"
KO_FILE="../kernel/nxp_simtemp.ko"
KERNEL_DEVICE="nxp_simtemp"
NORMAL_MODE="0"
NOISY_MODE="1"
RAMP_MODE="2"
EXPECT_COMMAND_REGEX=".+command\s\(.exit"

# Function executed when exit is triggered
cleanup_command() {
    echo "Executing command before exit..."

    # Kill the python coprocess if exists
    if kill -0 "${PYTHON_PROC_PID}" 2>/dev/null; then
        kill "${PYTHON_PROC_PID}" 2>/dev/null
    fi
    wait "${PYTHON_PROC_PID}" 2>/dev/null

    ./build.sh clean
    
    # Remove device if it's registered
    if lsmod | grep -q "$KERNEL_DEVICE"; then
        sudo rmmod $KERNEL_DEVICE
    fi
}

# Function to send a command to the python script
send_command() {
    local command_to_send="$1"
    echo "$command_to_send" >&$PYTHON_IN
}

# Trap the EXIT signal to run the cleanup_command function
trap cleanup_command EXIT

./build.sh

case "$1" in
    *)
        echo "Registering $KO_FILE in kernel"
        if [ -f "$KO_FILE" ]; then
            sudo insmod $KO_FILE
        else
            echo "The file '$KO_FILE' does not exist."
            exit 1
        fi

        if [ ! -f "$PYTHON_FILE" ]; then
            echo "The file '$PYTHON_FILE' does not exist."
            exit 1
        fi
        #start the python script as a coprocess
        coproc PYTHON_PROC { python3 $PYTHON_FILE }

        #COPROC_IN is the python script's standard input
        PYTHON_IN=${COPROC[1]}

        COMMAND_COUNT=0

        while IFS= read -r python_line <&"${PYTHON_PROC[0]}"; do
            echo "Python: $python_line"
            if [[ $python_line == "Current Configuration:" ]]; then
                echo "Python Script initialized"
            fi
            if [[ $python_line =~ $EXPECT_COMMAND_REGEX ]]; then
                if [[ $COMMAND_COUNT -eq 0 ]]; then
                    send_command "Write sample_mc 500"
                    COMMAND_COUNT=1
                elif [[ $COMMAND_COUNT -eq 1 ]]; then
                    send_command "Write threshold_mc 35000"
                    COMMAND_COUNT=2
                elif [[ $COMMAND_COUNT -eq 2 ]]; then
                    send_command "Write mode $NOISY_MODE"
                    COMMAND_COUNT=3
                elif [[ $COMMAND_COUNT -eq 3 ]]; then
                    sleep 5s
                    send_command "exit"
                    COMMAND_COUNT=4
                fi
            fi
            
            if [[ $python_line == "Program finished." ]]; then
                echo "Program finished!"
                break
            fi
        done
        ;;
    test)
        echo "Registering $KO_FILE in kernel"
        if [ -f "$KO_FILE" ]; then
            sudo insmod $KO_FILE
        else
            echo "The file '$KO_FILE' does not exist."
            exit 1
        fi

        if [ ! -f "$PYTHON_FILE" ]; then
            echo "The file '$PYTHON_FILE' does not exist."
            exit 1
        fi
        
        python3 $PYTHON_FILE "test"
        ;;
esac
