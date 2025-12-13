import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import re
import collections
import threading
import time

# --- AYARLAR ---
PORT = 'COM3'   # Arduino'nun bağlı olduğu portu kontrol et!
BAUD_RATE = 115200
X_LEN = 300     # Ekranda gösterilecek veri noktası sayısı

# --- GLOBAL DEĞİŞKENLER ---
# Yeni etiketlere göre değişken isimlerini güncelledik
data_rest = collections.deque([0] * X_LEN, maxlen=X_LEN)
data_index = collections.deque([0] * X_LEN, maxlen=X_LEN)   # Eskiden Fist idi
data_middle = collections.deque([0] * X_LEN, maxlen=X_LEN)  # Eskiden Finger idi

x_data = list(range(X_LEN))
is_running = True
current_status = "Bekleniyor..."

def read_serial_data():
    global current_status, is_running
    try:
        ser = serial.Serial(PORT, BAUD_RATE, timeout=1)
        print(f"{PORT} portuna bağlanıldı.")
        time.sleep(2) # Arduino reset sonrası toparlasın diye bekleme
    except Exception as e:
        print(f"Port hatası: {e}")
        is_running = False
        return

    while is_running:
        try:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                # --- KRİTİK GÜNCELLEME: REGEX ---
                # Gelen Veri Formatı: "Rest:0.83,Index:0.09,Middle:0.08"
                match = re.search(r"Rest:([0-9\.]+),Index:([0-9\.]+),Middle:([0-9\.]+)", line)
                
                if match:
                    try:
                        val_rest = float(match.group(1))
                        val_index = float(match.group(2))
                        val_middle = float(match.group(3))
                        
                        data_rest.append(val_rest)
                        data_index.append(val_index)
                        data_middle.append(val_middle)
                        
                        # Durum Belirleme (Eşik Değeri: 0.5)
                        if val_rest > 0.6: 
                            current_status = "DINLENME (REST)"
                        elif val_index > 0.6: 
                            current_status = "ISARET PARMAGI (INDEX)"
                        elif val_middle > 0.6: 
                            current_status = "ORTA PARMAK (MIDDLE)"
                        else: 
                            current_status = "Belirsiz..."
                    except ValueError:
                        pass # Hatalı float dönüşümü olursa atla
                    
        except Exception as e:
            print(f"Okuma hatası: {e}")
            break
            
    if ser.is_open:
        ser.close()
        print("Port kapatıldı.")

# --- GRAFİK AYARLARI ---
fig, ax = plt.subplots(figsize=(12, 6))
plt.subplots_adjust(bottom=0.2) # Alttaki yazı için yer aç

# Renkler: Yeşil(Rest), Mavi(Index), Kırmızı(Middle)
line_rest, = ax.plot(x_data, data_rest, color='green', label='Rest', alpha=0.6, linestyle='--')
line_index, = ax.plot(x_data, data_index, color='blue', label='Index (İşaret)', linewidth=2)
line_middle, = ax.plot(x_data, data_middle, color='red', label='Middle (Orta)', linewidth=2)

ax.set_ylim(-0.1, 1.1)
ax.set_title("Canlı EMG Sınıflandırma (3 Sınıf)")
ax.set_ylabel("Olasılık (0-1)")
ax.legend(loc='upper left')
ax.grid(True, alpha=0.3)

# Durum Yazısı
status_text = ax.text(0.5, -0.15, "Bekleniyor...", transform=ax.transAxes, 
                      ha="center", fontsize=16, fontweight='bold', color='black')

def update(frame):
    # Çizgileri güncelle
    line_rest.set_ydata(data_rest)
    line_index.set_ydata(data_index)
    line_middle.set_ydata(data_middle)
    
    # Durum yazısını güncelle ve renklendir
    status_text.set_text(current_status)
    
    if "INDEX" in current_status:
        status_text.set_color('blue')
    elif "MIDDLE" in current_status:
        status_text.set_color('red')
    else:
        status_text.set_color('green')

    return line_rest, line_index, line_middle, status_text

# Thread Başlat (Veri okuma arka planda dönsün)
thread = threading.Thread(target=read_serial_data)
thread.daemon = True
thread.start()

# Animasyonu Başlat
print("Grafik açılıyor...")
ani = animation.FuncAnimation(fig, update, interval=50, blit=True)
plt.show()

# Kapatılınca döngüyü durdur
is_running = False
thread.join()