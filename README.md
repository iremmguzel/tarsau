# Tarsau Archiver (tarsau)

Bu proje, Linux/Unix sistemleri için C dilinde geliştirilmiş, metin tabanlı dosyaları tek bir arşivde birleştiren ve orijinal izinleriyle geri yükleyen bir arşivleme yazılımıdır.

## 🚀 Özellikler
- **ASCII Kontrolü:** Sadece karakter başına 1 bayt (ASCII) olan metin dosyalarını arşivler.
- **Sınırlar:** Tek seferde en fazla **32 dosya** ve toplamda **200 MB** boyuta kadar işlem yapabilir.
- **İzinlerin Korunması:** Arşivden çıkarılan dosyalar, orijinal hallerindeki (read/write/execute) izinlerini korur.
- **Dinamik Dizin Yönetimi:** Arşiv açılırken belirtilen dizin mevcut değilse otomatik olarak oluşturulur.
- **Özel Format:** `.sau` uzantılı, özgün bir metaveri ve veri yapısına sahiptir.

## 🛠️ Kurulum ve Derleme
Projeyi derlemek için sisteminizde `gcc` ve `make` kurulu olmalıdır.

```bash
# Projeyi derle
make

# Derleme dosyalarını temizle
make clean
