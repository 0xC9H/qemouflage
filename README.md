<h1 align="center">qemouflage</h1>

<p align="center">
  <img src="https://img.shields.io/badge/language-C-blue?style=for-the-badge" alt="C">
  <img src="https://img.shields.io/badge/platform-Windows-0078D6?style=for-the-badge&logo=windows" alt="Windows">
  <img src="https://img.shields.io/badge/category-red_team_tooling-red?style=for-the-badge" alt="Red Team">
  <img src="https://img.shields.io/badge/compiler-mingw--w64-orange?style=for-the-badge" alt="mingw-w64">
  <img src="https://img.shields.io/badge/status-proof_of_concept-yellow?style=for-the-badge" alt="POC">
</p>

<p align="center">
  Deploy a fully provisioned Alpine Linux VM inside QEMU on a Windows host.<br>
  No admin privileges. No EDR visibility into the guest. Pure TCG emulation.
</p>

---

## Overview

Qemouflage is a proof of concept that demonstrates how to run an undetectable Linux implant on a Windows machine with standard user privileges.

On first run, the tool downloads QEMU and the latest Alpine cloud image, provisions the guest via cloud-init, and exposes:

- **SSH access** to the guest (`ssh -p 2222 svc@127.0.0.1`)
- **Host command execution** from the guest via `winexec` (reverse SSH tunnel + management server)
- **Optional C2 deployment** dropped and executed inside the guest on every boot

Everything lives under `%LOCALAPPDATA%\<WORK_DIR_NAME>\`. No services, no drivers, no registry keys.

## Prerequisites

| Package | Install |
|---------|---------|
| `x86_64-w64-mingw32-gcc` | `apt install gcc-mingw-w64-x86-64` |
| `x86_64-w64-mingw32-windres` | `apt install binutils-mingw-w64-x86-64` |

## Build

```bash
# Compile PE resources (version info, manifest, icon)
x86_64-w64-mingw32-windres --preprocessor-arg='-include' --preprocessor-arg='config.h' resources.rc -o resources.o

# Build
x86_64-w64-mingw32-gcc -O0 -g -include config.h -o wsconfig.exe qemouflage.c resources.o -lwinhttp -lws2_32 -luser32
```

## Configuration

All settings are compile-time. Edit `config.h` and rebuild.

<details>
<summary><b>Core</b></summary>

| Define | Default | Description |
|--------|---------|-------------|
| `WORK_DIR_NAME` | `AppServices` | Directory name under `%LOCALAPPDATA%` |
| `VM_RAM_MB` | `2048` | Guest RAM in MB |
| `VM_SMP` | `2` | Number of vCPUs |
| `DISK_GROW` | `+8G` | Extra space added to the qcow2 before first boot |

</details>

<details>
<summary><b>Network</b></summary>

| Define | Default | Description |
|--------|---------|-------------|
| `SSH_HOST_PORT` | `2222` | Host port for SSH forwarding (auto-increments if busy) |
| `EXEC_PORT` | `49152` | Host port for the management server |
| `GUEST_USER` | `svc` | Username created in the guest |
| `GUEST_PASSWORD` | `Ks8#mP2x` | Password for the guest user (also used for root) |
| `FILE_SSH_KEY` | `id_ed25519` | Filename for the auto-generated ed25519 keypair |

</details>

<details>
<summary><b>Guest payload</b></summary>

| Define | Default | Description |
|--------|---------|-------------|
| `UTILITY_URL` | `""` | URL of a binary to download into the guest (empty = disabled) |
| `UTILITY_GUEST_PATH` | `/usr/local/bin/appserviced` | Where to store it in the guest filesystem |

</details>

<details>
<summary><b>PE metadata</b></summary>

Adapt these per engagement to match your cover story.

| Define | Default | Description |
|--------|---------|-------------|
| `RC_COMPANY` | `Contoso Ltd.` | CompanyName in VERSIONINFO |
| `RC_DESCRIPTION` | `Workspace Configuration Utility` | FileDescription |
| `RC_PRODUCT` | `Contoso Workspace Tools` | ProductName |
| `RC_FILENAME` | `wsconfig.exe` | OriginalFilename |
| `RC_VERSION_STR` | `3.4.1.0` | Version string |
| `RC_VERSION_NUM` | `3,4,1,0` | Version numeric tuple |

</details>

<details>
<summary><b>Download URLs</b></summary>

| Define | Description |
|--------|-------------|
| `URL_7ZR` / `URL_7ZFULL` | 7-Zip standalone + full package URLs |
| `QEMU_BASE_URL` | QEMU release index (auto-detects latest version) |
| `ALPINE_BASE_URL` | Alpine cloud image index |
| `QEMU_FALLBACK` / `ALPINE_FALLBACK` | Hardcoded filenames used when index parsing fails |
| `HTTP_UA` | User-Agent for all outbound HTTP requests |

</details>

## Project structure

```
qemouflage/
├── qemouflage.c      # Main source (single file)
├── config.h           # All compile-time configuration
├── resources.rc       # PE resources (VERSIONINFO, icon, manifest, dialogs, menus)
├── app.manifest       # Application manifest (asInvoker, DPI aware)
└── app.ico            # Application icon
```

## Disclaimer

*This tool is intended for authorized Red Team engagements and security research only.*
