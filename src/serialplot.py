import serial
import matplotlib.pyplot as plt
from collections import deque

# Portu kendinize göre düzeltin (Aygıt Yöneticisinden bakın)
# Windows: 'COM3', Mac/Linux: '/dev/ttyUSB0'
ser = serial.Serial('COM3', 115200) 

x_len = 200 # Grafikte gösterilecek nokta sayısı
y1_data = deque([0] * x_len, maxlen=x_len)
y2_data = deque([0] * x_len, maxlen=x_len)

plt.ion() # İnteraktif mod
fig, ax = plt.subplots()
line1, = ax.plot(y1_data, label='Action Prob')
line2, = ax.plot(y2_data, label='Threshold', linestyle='--')
ax.set_ylim(0, 1.2) # Y ekseni aralığı
plt.legend()

print("Veri bekleniyor...") # Başlangıç kontrolü

# 3. Çizgi için deque ekleyin
y3_data = deque([0] * x_len, maxlen=x_len) 
line3, = ax.plot(y3_data, label='Sinyal Girisi', color='green', alpha=0.3) # Yeşil ve silik çizgi

while True:
    if ser.in_waiting:
        try:
            line = ser.readline().decode('utf-8').strip()
            parts = line.split(',')
            
            # ARTIK 3 DEĞER BEKLİYORUZ
            if len(parts) == 3: 
                input_val = float(parts[0])
                p0 = float(parts[1])
                p1 = float(parts[2])

                y1_data.append(p0)       # Sınıf 0
                y2_data.append(p1)       # Sınıf 1
                y3_data.append(input_val) # Sinyal (Yeşil)

                line1.set_ydata(y1_data)
                line2.set_ydata(y2_data)
                line3.set_ydata(y3_data)
                
                # Y eksenini otomatiğe alalım ki sinyali görelim
                ax.relim()
                ax.autoscale_view()
                
                plt.pause(0.01)
            else:
                print("FORMAT HATASI: Virgul yok veya eksik veri.")

        except ValueError as e:
            print(f"SAYIYA ÇEVİRME HATASI: '{line}' verisi sayıya dönemedi. Hata: {e}")