#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_FILES 32
#define MAX_TOTAL_SIZE (200 * 1024 * 1024)

/**
 * Dosyanın ASCII formatında olup olmadığını denetler
 * Giriş dosyaları yalnızca metin dosyaları olabilir
 */
int is_ascii(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        // Karakterlerin 0-127 (ASCII) aralığında olması kontrol edilir
        if (c < 0 || c > 127) {
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return 1;
}

/**
 * Dosya boyutunu sistem üzerinden sorgular
 * Toplam boyutun 200 MB sınırını aşmaması kontrolünde kullanılır
 */
long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_size;
    return -1;
}

int main(int argc, char *argv[]) {
    // Komut satırı argüman sayısı kontrolü
    if (argc < 2) {
        printf("Kullanim: tarsau -b [dosyalar] -o [cikti] veya tarsau -a [arsiv] [dizin]\n");
        return 1;
    }

    // tarsau-b: Arşivleme Modülü
    if (strcmp(argv[1], "-b") == 0) {
        int file_count = 0;
        long total_size = 0;
        char *output_filename = "a.sau"; // Varsayılan dosya adı

        // Çıktı dosyasının adını parametreden belirler
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                output_filename = argv[i + 1];
                break;
            }
        }

        // Giriş dosyalarını doğrular ve sınırları kontrol eder
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) { i++; continue; }
            file_count++;
            
            // Dosya sayısı sınırı kontrolü
            if (file_count > MAX_FILES) {
                printf("Hata: En fazla 32 dosya arsivlenebilir!\n");
                return 1;
            }
            
            // ASCII ve metin formatı kontrolü
            if (!is_ascii(argv[i])) {
                printf("%s giriş dosyasının formatı uyumsuzdur!\n", argv[i]);
                return 1;
            }
            
            long current_size = get_file_size(argv[i]);
            if (current_size == -1) return 1;
            total_size += current_size;
        }

        // Toplam boyut sınırı kontrolü
        if (total_size > MAX_TOTAL_SIZE) {
            printf("Hata: Giriş dosyalarının toplam boyutu 200 MB'ı geçemez!\n");
            return 1;
        }

        FILE *archive_fp = fopen(output_filename, "w");
        if (!archive_fp) return 1;

        // 1. Organizasyon Bölümü Hazırlığı (Metadata)
        char metadata[10000] = ""; 
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) { i++; continue; }
            struct stat st;
            stat(argv[i], &st);
            char file_info[512];
            // Kayıt yapısı: Dosya adı, izinler, boyut
            sprintf(file_info, "%s,%o,%ld|", argv[i], st.st_mode & 0777, (long)st.st_size);
            strcat(metadata, file_info);
        }

        // Arşiv dosyasının ilk 10 baytı metadata boyutunu içerir
        char header[64];
        sprintf(header, "%010ld", (long)strlen(metadata));
        fwrite(header, sizeof(char), 10, archive_fp);
        
        // Kayıtlar '|' karakteriyle ayrılarak yazılır
        fwrite(metadata, sizeof(char), strlen(metadata), archive_fp);

        // 2. Arşivlenmiş Dosyaların Veri Bölümü
        // Dosyalar ayırıcı olmadan ardı ardına eklenir
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) { i++; continue; }
            FILE *input_fp = fopen(argv[i], "r");
            if (input_fp) {
                int c;
                while ((c = fgetc(input_fp)) != EOF) fputc(c, archive_fp);
                fclose(input_fp);
            }
        }
        fclose(archive_fp);
        printf("Dosyalar birleştirildi.\n");
    } 
    // tarsau-a: Arşivden Çıkarma Modülü
    else if (strcmp(argv[1], "-a") == 0) {
        if (argc < 3 || argc > 4) {
            printf("Kullanim: tarsau -a [arsiv] [dizin]\n");
            return 1;
        }
        
        char *archive_name = argv[2];
        char *target_dir = (argc == 4) ? argv[3] : "."; 

        // Arşiv dosyası adı ve uzantı doğrulaması
        if (strstr(archive_name, ".sau") == NULL) {
            printf("Arşiv dosyası uygunsuz veya bozuk!\n");
            return 1;
        }

        FILE *archive_fp = fopen(archive_name, "r");
        if (!archive_fp) {
            printf("Arşiv dosyası uygunsuz veya bozuk!\n");
            return 1;
        }

        // Metadata boyutu ilk 10 bayttan okunur
        char header[11];
        if (fread(header, sizeof(char), 10, archive_fp) < 10) {
            printf("Arşiv dosyası uygunsuz veya bozuk!\n");
            fclose(archive_fp);
            return 1;
        }
        header[10] = '\0';
        long metadata_size = atol(header);

        // Organizasyon bilgilerinin belleğe alınması
        char *metadata = malloc(metadata_size + 1);
        fread(metadata, sizeof(char), metadata_size, archive_fp);
        metadata[metadata_size] = '\0';

        // Belirtilen dizin yoksa oluşturulur
        if (argc == 4) mkdir(target_dir, 0777); 

        char file_list_msg[2048] = ""; 
        char *metadata_copy = strdup(metadata);
        char *file_record = strtok(metadata_copy, "|");
        long current_offset = 10 + metadata_size; 

        int first = 1;
        while (file_record != NULL) {
            char f_name[256], f_mode_str[16];
            long f_size;
            // Alanlar virgülle ayrılmıştır
            if (sscanf(file_record, "%[^,],%[^,],%ld", f_name, f_mode_str, &f_size) == 3) {
                if (!first) strcat(file_list_msg, ", ");
                else first = 0;
                strcat(file_list_msg, f_name);

                char full_path[512];
                sprintf(full_path, "%s/%s", target_dir, f_name);
                FILE *out_fp = fopen(full_path, "w");
                if (out_fp) {
                    // Veri bölümüne konumlanarak dosya içeriği çıkarılır
                    fseek(archive_fp, current_offset, SEEK_SET);
                    for (long j = 0; j < f_size; j++) {
                        int byte = fgetc(archive_fp);
                        if (byte != EOF) fputc(byte, out_fp);
                    }
                    fclose(out_fp);
                    
                    // Dosya izinleri orijinal haliyle geri yüklenir
                    mode_t mode = strtol(f_mode_str, NULL, 8);
                    chmod(full_path, mode);
                }
                current_offset += f_size;
            }
            file_record = strtok(NULL, "|");
        }
        
        // Çıkış mesajı formatı örneğe uygun şekilde düzenlenir
        printf("%s dizininde %s dosyaları açıldı.\n", target_dir, file_list_msg);
        free(metadata);
        free(metadata_copy);
        fclose(archive_fp);
    }
    return 0;
}
