import scipy.io
import numpy as np

# --- AYARLAR ---
MAT_FILE = 'S1_A1_E1.mat'
OUTPUT_H = 'test_vectors.h'
WINDOW_SIZE = 50  # RMS Pencere Boyutu (Yumuşatma için)
NUM_CHANNELS = 10 # Senin modelin 10 kanal bekliyor

print(f"Loading {MAT_FILE}...")
mat = scipy.io.loadmat(MAT_FILE)
emg_data = mat['emg']       # Shape: (101014, 10)
stimulus = mat['stimulus']  # Labels

# --- ÖZEL TEST SENARYOSU OLUŞTURUYORUZ ---
# Rastgele veri yerine, grafikte net görebileceğimiz bir senaryo seçiyoruz.
# Senaryo: Dinlenme (Class 0) -> Güçlü Hareket (Class 4) -> Dinlenme (Class 0)

# 1. Dinlenme Bölümü (İlk 200 örnek)
rest_data = emg_data[0:200, :NUM_CHANNELS]
rest_lbls = stimulus[0:200]

# 2. Güçlü Hareket Bölümü (Index 27300 civarında güçlü Class 4 var)
# Buradaki sinyal ortalaması 0.3 civarında (Rest'in 30 katı)
move_data = emg_data[27350:27550, :NUM_CHANNELS]
move_lbls = stimulus[27350:27550]

# 3. Verileri Birleştir
combined_data = np.vstack((rest_data, move_data, rest_data)) # Toplam 600 örnek
combined_lbls = np.vstack((rest_lbls, move_lbls, rest_lbls))

print(f"Raw Data Created. Shape: {combined_data.shape}")

# --- RMS (ROOT MEAN SQUARE) UYGULAMA ---
# Modeli beslemeden önce sinyali yumuşatıyoruz (Smoothing)
def calculate_rms(data, window):
    # Her kanal için kayan pencere ortalaması (RMS)
    processed = np.zeros_like(data)
    for ch in range(data.shape[1]):
        # Karesini al -> Ortalamasını al -> Karekökünü al
        signal_sq = data[:, ch] ** 2
        # Basit moving average (convolve ile)
        processed[:, ch] = np.sqrt(np.convolve(signal_sq, np.ones(window)/window, mode='same'))
    return processed

print("Applying RMS Smoothing...")
test_data_rms = calculate_rms(combined_data, WINDOW_SIZE)

# --- HEADER DOSYASI OLUŞTUR ---
print(f"Writing to {OUTPUT_H}...")
with open(OUTPUT_H, "w") as f:
    f.write(f"#ifndef TEST_VECTORS_H\n#define TEST_VECTORS_H\n\n")
    f.write(f"const int TEST_DATA_LEN = {len(test_data_rms)};\n")
    f.write(f"const int TEST_CHANNELS = {NUM_CHANNELS};\n\n")
    
    f.write(f"const float test_data[{len(test_data_rms)}][{NUM_CHANNELS}] = {{\n")
    for row in test_data_rms:
        f.write("  {")
        # 4.66 maksimum değerdi, veriyi 0-1 arasına çekmek için basit normalizasyon yapabiliriz.
        # Eğer modelin normalize edilmiş veri ile eğitildiyse, aşağıdaki '/ 5.0' satırını kullan.
        # Eğitilmediyse raw bırak. Şimdilik RAW bırakıyoruz ama yumuşatılmış.
        f.write(", ".join([f"{x:.5f}" for x in row])) 
        f.write("},\n")
    f.write("};\n\n")

    f.write(f"const int test_labels[{len(combined_lbls)}] = {{\n")
    f.write(", ".join([str(int(x[0])) for x in combined_lbls]))
    f.write("\n};\n\n")
    
    f.write("#endif")

print("DONE! Yeni test_vectors.h hazir. Lutfen ESP32'ye yukleyin.")