import sys
import os
import struct 
import select
import multiprocessing
import re
import time
import datetime
import errno
import threading

write_regex= r"^(write)\s+(\w+)\s+(\d+)"

#constants for paths
SYSFS_PATH = '/sys/class/misc/simtemp/'
CONFIG_PATHS = {
    'sampling_mc': SYSFS_PATH + 'simtemp_sampling',
    'threshold_mc': SYSFS_PATH + 'simtemp_threshold',
    'mode': SYSFS_PATH + 'simtemp_mode'
}
DEVICE_PATH = '/dev/simtemp'

THRESHOLD_CROSSED = 0x2
THRESHOLD_CROSSED_BIT = 1

NORMAL_MODE = 0
NOISY_MODE = 1
RAMP_MODE = 2

#struct simtemp_sample layout: timestamp_ns (uint64), temp_mC (int32), flags (uint32)
SAMPLE_STRUCT_FORMAT = 'Q i I' # Q: uint64, i: int32, I: uint32
SAMPLE_STRUCT_SIZE = struct.calcsize(SAMPLE_STRUCT_FORMAT)

def move_cursor_up(n=1):
    sys.stdout.write(f"\033[{n}A")

def save_cursor():
    sys.stdout.write("\033[s")

def restore_cursor():
    sys.stdout.write("\033[u")

def insert_line():
    sys.stdout.write("\033[L")

def conver_ktime_to_iso(timestamp_ns):
    now_epoch = time.time()                            # current wall-clock time (UTC)
    uptime_seconds = time.monotonic()                  # monotonic time since boot
    boot_time_epoch = now_epoch - uptime_seconds       # estimate of boot time in epoch seconds
    ktime_seconds = timestamp_ns / 1e9                 # convert timestamp_ns to int and seconds
    event_time_epoch = boot_time_epoch + ktime_seconds # compute the absolute timestamp of the event
    event_time = datetime.datetime.fromtimestamp(event_time_epoch, tz=datetime.timezone.utc) #convert to a timezone-aware datetime in UTC
    iso_time_str = event_time.isoformat(timespec='milliseconds')
    iso_time_str = iso_time_str.replace("+00:00", "Z") # replace "+00:00" with "Z" for strict ISO 8601 format

    return iso_time_str

def read_config(sysfs_file):
    try:
        with open(sysfs_file, 'r') as f:
            config_value = f.read().strip()
    except Exception as e:
        print(f"Warning: Could not read from {sysfs_file}: {e}")
        return -errno.EINVAL
    return config_value

def write_config(sysfs_file, value):
    try:
        with open(sysfs_file, 'w') as f:
            f.write(str(value))
    except Exception as e:
        print(f"Warning: Could not write {value} to {sysfs_file}: {e}")
        return -errno.EINVAL
    return 0
    
def read_sample(fd):
    data = os.read(fd, SAMPLE_STRUCT_SIZE)
    if len(data) != SAMPLE_STRUCT_SIZE:
        raise IOError(f"Incomplete read: expected {SAMPLE_STRUCT_SIZE} bytes, got {len(data)}")
    timestamp_ns, temp_mC, flags = struct.unpack(SAMPLE_STRUCT_FORMAT, data)
    return timestamp_ns, temp_mC, flags

def print_sample_process(stop_event):
    fd = os.open(DEVICE_PATH, os.O_RDONLY | os.O_NONBLOCK)
    ep = select.epoll()
    ep.register(fd, select.EPOLLIN)

    try:
        while not stop_event.is_set():
            events = ep.poll(timeout=-1) #block until something is ready
            for fileno, event in events:
                if fileno == fd and event &select.EPOLLIN:
                    try:
                        timestamp_ns, temp_mC, flags = read_sample(fd)
                        iso_time_str = conver_ktime_to_iso(timestamp_ns)
                        alert = (flags & THRESHOLD_CROSSED) >> THRESHOLD_CROSSED_BIT
                        save_cursor()
                        insert_line()
                        sys.stdout.write("\r")
                        print(f"{iso_time_str}, Temp: {temp_mC/1000}°C, Alert: {alert}")
                        restore_cursor()
                        sys.stdout.flush()
                    except Exception as e:
                        print(f"Error reading sample: {e}")
    finally:
        ep.unregister(fd)
        os.close(fd)

def print_sample_test(test_number, ep, fd):
    i = 0
    test_done = 0
    count = 0
    if test_number == 4:
        number_of_samples = 1500
    else:
        number_of_samples = 20

    while i < number_of_samples:
        events = ep.poll(timeout=-1)
        for fileno, event in events:
            try:
                timestamp_ns, temp_mC, flags = read_sample(fd)
                i += 1
                if(test_number == 2 and test_done == 0):
                    if(i == 1):
                        init_time = timestamp_ns
                    time_since_test = timestamp_ns - init_time
                    if(time_since_test >= 1e9):
                        count = i
                        test_done = 1
                elif(test_number == 3):
                    if ((flags & THRESHOLD_CROSSED) and test_done == 0):
                        count = i
                        test_done = 1
                elif(test_number == 4 and test_done == 0):
                    if(i == 1):
                        init_time = timestamp_ns
                    time_since_test = timestamp_ns - init_time
                    if(time_since_test >= 1e9):
                        count = i
                        test_done = 1
            except Exception as e:
                print(f"Error reading sample: {e}")
        
    return count

def read_task():
    for _ in range(5):
        sampling = read_config(CONFIG_PATHS["sampling_mc"])
        threshold = read_config(CONFIG_PATHS["threshold_mc"])
        mode = read_config(CONFIG_PATHS["mode"])
        print(f"Read Thread read values: sampling = {sampling}, threshold = {threshold}, mode = {mode}")
        time.sleep(0.1)

def write_task():
    sampling = 101
    threshold = 46000
    mode = 0
    for i in range(5):
        write_config(CONFIG_PATHS["sampling_mc"], sampling)
        write_config(CONFIG_PATHS["threshold_mc"], threshold)
        write_config(CONFIG_PATHS["mode"], mode)
        print(f"Write Thread wrote values: sampling = {sampling}, threshold = {threshold}, mode = {mode}")
        sampling += 1
        threshold += 100
        if mode <= 1:
            mode += 1
        else:
            mode = 0
        time.sleep(0.15)

def main():
    if len(sys.argv) == 1:
        #Read current config and display it
        print ("Current Configuration:")
        sampling_mc = read_config(CONFIG_PATHS['sampling_mc'])
        threshold_mc = read_config(CONFIG_PATHS['threshold_mc'])
        mode = read_config(CONFIG_PATHS['mode'])
        print(f"sampling_mc = {sampling_mc}")
        print(f"threshold_mc = {threshold_mc}")
        print(f"mode = {mode}")

        # create stop event
        stop_event = multiprocessing.Event()
        print("Queue created")

        # create and star sample thread
        sample_process = multiprocessing.Process(target=print_sample_process, args=(stop_event,))
        sample_process.daemon = True #allows the program to exit when the main thread does
        
        sample_process.start()
        print("sample process created")

        print("Program started. Listening for updates and user input...")
        try:
            # Waits for user input and puts it into the queue
            while not stop_event.is_set():
                try:
                    print("Input started...")
                    command = input("Enter a command ('exit' to stop): ")
                    if command.lower() == "exit":
                        stop_event.set()
                    regex_match = re.match(write_regex, command.lower())
                    if regex_match:
                        input_cmd, input_config, input_value = regex_match.groups()
                    print(f"Received {input_cmd}, {input_config} and {input_value}")
                    if input_cmd == "write":
                        write_config(CONFIG_PATHS[input_config], input_value)
                        if(input_config == "mode"):
                            print("Mode changed")
                except EOFError:
                    #handle cases where input stream is closed. e.g. in automated tests
                    break
                except KeyboardInterrupt:
                    break
            print("Input thread stopping...")
        except KeyboardInterrupt:
            print("\nMain thread received Ctrl+c. Stopping...")
            stop_event.set()
        except Exception as e:
            print(f"Main thread caught error: {e}")
            stop_event.set()
        finally:
            # Wait for threads to finish ther cleanup
            sample_process.join()
            #input_process.join()
        print("Program finished.")   
        
    # Test Mode 
    elif len(sys.argv) == 2 and sys.argv[1] == "test":
        GREEN = '\033[92m'
        RED = '\033[91m'
        RESET = '\033[0m'
        T1 = "NOT PASSED"
        T2 = "NOT PASSED"
        T3 = "NOT PASSED"
        T4 = "NOT PASSED"
        T5 = RED + "NOT PASSED" + RESET
        print("Test Mode enabled...")
        fd = os.open(DEVICE_PATH, os.O_RDONLY | os.O_NONBLOCK)
        ep = select.epoll()
        ep.register(fd, select.EPOLLIN)
        # Test 1.
        print("Test 1: Load/unload. Verify if /dev/simtemp exists") 
        # check if file exists
        if os.path.exists("/dev/simtemp"):
            print("T1: Passed /dev/simtemp exists")
            T1 = GREEN + "PASSED" + RESET
        else:
            print("T1: Not passed /dev/simtemp does not exists")
            T1 = RED + "NOT PASSED" + RESET

        # Test 2
        write_config(CONFIG_PATHS["sampling_mc"], 100) # set sampling at 100ms
        count = print_sample_test(2, ep, fd) # run for 10 samples
        print("Test 2: Verifying sampling_ms = 100ms") 
        # Check if time meets requirement
        if(count <= 11 and count >= 9):
            print(f"T2: Passed. Samples: {count}/s")
            T2 = GREEN + "PASSED" + RESET
        else:
            print(f"T2: Not Passed. Samples: {count}/s")
            T2 = RED + "NOT PASSED" + RESET

        # Test 3
        print("Test 3: Verifying threshold. mode = Normal mode, threshold = 24.99°C")
        write_config(CONFIG_PATHS["mode"], NORMAL_MODE)   # Change sampling mode to Normal Mode
        write_config(CONFIG_PATHS["threshold_mc"], 24990) # Change threshold to 24.99°C (below mean of 25°C)
        count = print_sample_test(3, ep, fd)
        if(count <= 3):
            print(f"T3: Passed. Samples until thresold changed: {count}")
            T3 = GREEN + "PASSED" + RESET
        else:
            print(f"T3: Not Passed. Samples until threshold changed: {count}")
            T3 = RED + "NOT PASSED" + RESET

        # Test 4
        print("Test 4: Verifying error paths and fast sampling at 1ms")
        write_return = write_config(SYSFS_PATH + "mod", NOISY_MODE)
        write_config(CONFIG_PATHS["threshold_mc"], 45000)
        write_config(CONFIG_PATHS["mode"], RAMP_MODE)
        write_config(CONFIG_PATHS["sampling_mc"], 1)
        count = print_sample_test(4, ep, fd)
        
        if(write_return == -errno.EINVAL and count <= 1100 and count >= 900):
            print(f"T4: Passed. Samples: {count}/s. Write returned {write_return} expected {-errno.EINVAL}(EINVAL)")
            T4 = GREEN + "PASSED" + RESET
        else:
            print(f"T4: Not Passed. Samples: {count}/s. Write returned {write_return} expected {-errno.EINVAL}")
            T4 = RED + "NOT PASSED" + RESET

        ep.unregister(fd)
        os.close(fd)
        
        #Test 5
        print("Test 5: Run read and config write concuttently.")
        thread_read = threading.Thread(target=read_task)
        thread_write = threading.Thread(target=write_task)

        thread_read.start()
        thread_write.start()

        # timeout is used to detect deadelocks. If a thread does not finish in time, the test fails.
        thread_read.join(timeout=2)
        thread_write.join(timeout=2)

        assert not thread_read.is_alive(), "Reader thread did not finished in time"
        assert not thread_write.is_alive(), "Writer thread did not finished in time"
        print("T5: Passed, No deadlocks, threads exited safely")
        T5 = GREEN + "PASSED" + RESET

        print(f"T1: {T1}")
        print(f"T2: {T2}")
        print(f"T3: {T3}")
        print(f"T4: {T4}")
        print(f"T5: {T5}")
    # Error in argument
    else:
        print("Error: Wrong Argument")
        
if __name__ == "__main__":
    main()
    