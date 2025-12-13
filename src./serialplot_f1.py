import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import re
import collections

# --- 1. AYARLAR ---
# Windows için 'COM3', 'COM4' vb.
# Mac/Linux için '/dev/tty.usbmodem...' veya '/dev/ttyUSB0'
PORT = '/dev/cu.usbmodem1101'  # <--- BURAYI KENDİ PORTUNUZLA DEĞİŞTİRİN
BAUD_RATE = 115200
X_LEN = 200  # Ekranda gösterilecek veri sayısı (Pencere genişliği)

# --- 2. SERİ PORT BAĞLANTISI ---
try:
    ser = serial.Serial(PORT, BAUD_RATE, timeout=0.1)
    print(f"Baglanti basarili: {PORT}")
    print("Arduino Serial Monitor'u KAPATTIGINIZDAN emin olun!")
    print("Veri bekleniyor...")
except Exception as e:
    print(f"HATA: Port acilamadi ({PORT}).")
    print(f"Detay: {e}")
    print("\nÇözüm: \n1. Arduino IDE'deki Serial Monitor'ü kapatın.\n2. USB kablosunu çıkarıp takın.\n3. Port isminin doğru olduğunu kontrol edin.")
    exit()

# --- 3. VERİ HAVUZLARI ---
# Deque: Dolunca en eski veriyi otomatik silen liste
data_ham = collections.deque([0] * X_LEN, maxlen=X_LEN)
data_filtre = collections.deque([0] * X_LEN, maxlen=X_LEN)
x_data = list(range(X_LEN))

# --- 4. GRAFİK AYARLARI ---
fig, ax = plt.subplots(figsize=(12, 6))
plt.subplots_adjust(bottom=0.2) # Altta yazı için boşluk bırak

# Çizgileri Tanımla
line_ham, = ax.plot(x_data, data_ham, color='lightgray', label='Ham Model Çıktısı (Titrek)', linestyle='--')
line_filtre, = ax.plot(x_data, data_filtre, color='red', label='Filtreli Karar (AI)', linewidth=2)
line_esik = ax.axhline(y=0.2, color='green', linestyle=':', label='Hareket Eşiği (0.2)', alpha=0.8)

# Eksen Sınırları
ax.set_ylim(-0.1, 1.1) # Olasılık 0 ile 1 arasındadır
ax.set_title(f"ESP32 EMG AI Model Analizi ({PORT})")
ax.set_ylabel("Hareket Olasılığı")
ax.grid(True, alpha=0.3)
ax.legend(loc='upper left')

# Durum Metni (Grafiğin altına anlık durumu yazar)
status_text = ax.text(0.5, -0.15, "Bekleniyor...", transform=ax.transAxes, ha="center", fontsize=12, color="blue")

# --- 5. GÜNCELLEME FONKSİYONU ---
def update(frame):
    global status_text
    
    # Serial Buffer'daki tüm veriyi oku (Gecikmeyi önler)
    while ser.in_waiting:
        try:
            # Satırı oku ve temizle
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            
            # ESP32'den gelen format şuna benzer: 
            # "Ham: 0.85 | Filtreli: 0.22"
            
            # Regex ile sayıları çekiyoruz (Daha güvenli)
            match = re.search(r"Ham:\s*([0-9\.]+)\s*\|\s*Filtreli:\s*([0-9\.]+)", line)
            
            if match:
                val_ham = float(match.group(1))
                val_filtre = float(match.group(2))
                
                # Listelere ekle
                data_ham.append(val_ham)
                data_filtre.append(val_filtre)
                
                # Durum yazısını güncelle
                durum = "HAREKET ALGILANDI" if val_filtre > 0.2 else "Dinlenme"
                renk = "green" if val_filtre > 0.6 else "black"
                status_text.set_text(f"Ham: {val_ham:.2f} | Filtreli: {val_filtre:.2f} -> {durum}")
                status_text.set_color(renk)
                
            else:
                # Eğer veri geliyor ama format uymuyorsa konsola bas (Debug için)
                if len(line) > 2:
                    print(f"Okunamadı: {line}")

        except ValueError:
            pass
        except Exception as e:
            print(f"Hata: {e}")

    # Grafiği çiz
    line_ham.set_ydata(data_ham)
    line_filtre.set_ydata(data_filtre)
    
    return line_ham, line_filtre, status_text

# --- 6. ANİMASYONU BAŞLAT ---
# interval=20 -> 20ms'de bir günceller (50 FPS)
ani = animation.FuncAnimation(fig, update, interval=20, blit=True, cache_frame_data=False)

print("Grafik başlatılıyor... (Kapatmak için pencereyi kapatın)")
plt.show()

# Çıkışta portu kapat
ser.close()
print("Port kapatıldı.")