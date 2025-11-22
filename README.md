# trigger
program ringan berbasis C yang berfungsi untuk menjaga file tetap ada dan selalu diperbarui otomatis


# restore-daemon

Program ringan berbasis C untuk menjaga file penting agar selalu tersedia.  
Script ini bekerja secara otomatis untuk menyalin file dari sumber ke tujuan setiap beberapa detik.

## ✨ Fitur
- Menyalin file sumber ke lokasi tujuan setiap 3 detik
- Membuat folder yang hilang secara otomatis (mirip `mkdir -p`)
- Menimpa file tujuan jika sudah ada (overwrite)
- Menyimpan log ke file `restore.log`
- Dapat berjalan terus hingga dihentikan secara manual (`kill` atau `Ctrl + C`)
- Opsi tambahan: otomatis mengunduh file jika sumber hilang (melalui URL)

## ⚙️ Compile
```bash
gcc -O2 -Wall -o trigger trigger.c >> C99
gcc -O2 -Wall -std=c99 -o lol lol.c >> C 89/90
chmod +x restore
nohup ./restore > ./restore.log 2>&1 &

python
nohup /usr/bin/python3 /tmp/restore.py >/tmp/smokykontol.log 2>&1 &

php
nohup php < /var/cache/php-eaccelerator/phpfm.php >/dev/null 2>&1 &
