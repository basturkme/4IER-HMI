import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import re
import collections
import threading
import time

# --- SETTINGS ---
PORT = 'COM3'   # Check your COM port
BAUD_RATE = 115200
X_LEN = 300     # Display length

# --- GLOBAL VARIABLES ---
data_rest = collections.deque([0] * X_LEN, maxlen=X_LEN)
data_index = collections.deque([0] * X_LEN, maxlen=X_LEN)
data_middle = collections.deque([0] * X_LEN, maxlen=X_LEN)
data_test_vector = collections.deque([0] * X_LEN, maxlen=X_LEN)

x_data = list(range(X_LEN))
is_running = True
current_status = "Waiting..."

def read_serial_data():
    global current_status, is_running
    try:
        ser = serial.Serial(PORT, BAUD_RATE, timeout=1)
        print(f"Connected to {PORT}.")
        time.sleep(2) 
    except Exception as e:
        print(f"Port Error: {e}")
        is_running = False
        return

    while is_running:
        try:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                # --- CRITICAL FIX: REGEX ---
                # Old (Failed): r"Test:([0-9\.]+),Rest:..." (Expected comma)
                # New (Robust): Uses '.*?' to match anything (or nothing) between numbers and labels
                match = re.search(r"Test:([0-9\.]+).*?Rest:([0-9\.]+).*?Index:([0-9\.]+).*?Middle:([0-9\.]+)", line)
                
                if match:
                    try:
                        val_test = float(match.group(1))
                        val_rest = float(match.group(2))
                        val_index = float(match.group(3))
                        val_middle = float(match.group(4))
                        
                        data_test_vector.append(val_test)
                        data_rest.append(val_rest)
                        data_index.append(val_index)
                        data_middle.append(val_middle)
                        
                        # Status Logic
                        if val_rest > 0.6: 
                            current_status = "(REST)"
                        elif val_index > 0.6: 
                            current_status = "(INDEX)"
                        elif val_middle > 0.6: 
                            current_status = "(MIDDLE)"
                        else: 
                            current_status = "Uncertain..."
                            
                    except ValueError:
                        pass 
                else:
                    # Debugging: If match fails, show us why (print first 50 chars)
                    # print(f"Regex Failed on: {line[:50]}") 
                    pass

        except Exception as e:
            print(f"Read Error: {e}")
            break
            
    if ser.is_open:
        ser.close()
        print("Port closed.")

# --- PLOT SETTINGS ---
fig, ax = plt.subplots(figsize=(12, 6))
plt.subplots_adjust(bottom=0.2)

# Left Axis (Probabilities)
line_rest, = ax.plot(x_data, data_rest, color='green', label='Rest Prob', alpha=0.6, linestyle='--')
line_index, = ax.plot(x_data, data_index, color='blue', label='Index Prob', linewidth=2)
line_middle, = ax.plot(x_data, data_middle, color='red', label='Middle Prob', linewidth=2)

ax.set_ylim(-0.1, 1.1)
ax.set_ylabel("Probability (0-1)")
ax.set_title("Real Time EMG Classification & Test Signal")
ax.grid(True, alpha=0.3)

# Right Axis (Test Vector)
ax2 = ax.twinx()
line_test, = ax2.plot(x_data, data_test_vector, color='black', label='Test Input', alpha=0.3, linewidth=1)
ax2.set_ylabel("Input Amplitude")

# Combine Legends
lines_1, labels_1 = ax.get_legend_handles_labels()
lines_2, labels_2 = ax2.get_legend_handles_labels()
ax.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper left')

# Status Text
status_text = ax.text(0.5, -0.15, "Waiting...", transform=ax.transAxes, 
                      ha="center", fontsize=16, fontweight='bold', color='black')

def update(frame):
    line_rest.set_ydata(data_rest)
    line_index.set_ydata(data_index)
    line_middle.set_ydata(data_middle)
    
    line_test.set_ydata(data_test_vector)
    ax2.relim()
    ax2.autoscale_view()

    status_text.set_text(current_status)
    if "INDEX" in current_status:
        status_text.set_color('blue')
    elif "MIDDLE" in current_status:
        status_text.set_color('red')
    elif "REST" in current_status:
        status_text.set_color('green')
    else:
        status_text.set_color('black')

    return line_rest, line_index, line_middle, line_test, status_text

thread = threading.Thread(target=read_serial_data)
thread.daemon = True
thread.start()

print("Opening Plot...")
ani = animation.FuncAnimation(fig, update, interval=50, blit=True)
plt.show()

is_running = False
thread.join()