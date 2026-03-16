# Build kstuff.elf on Windows (SquashFS + USB support)

kstuff must be built on **Linux x86_64** (the payload uses x86 asm and the lib links Linux ELF objects). On Windows, use **WSL2 with Ubuntu**.

---

## Option A: WSL2 + Ubuntu (recommended on Windows)

### 1. Install WSL2 and Ubuntu

- In PowerShell (Admin): `wsl --install`
- Or: Install “Ubuntu” from Microsoft Store, then set WSL2: `wsl --set-default-version 2`
- Restart if asked, then open **Ubuntu** from the Start menu.

### 2. Inside Ubuntu (WSL2)

```bash
# Update and install build deps (x86_64)
sudo apt update
sudo apt install -y build-essential xxd yasm nasm python3 make git wget

# Download and install PS5 Payload SDK
cd ~
wget https://github.com/ps5-payload-dev/pacbrew-repo/releases/latest/download/ps5-payload-dev.tar.gz
sudo tar xf ps5-payload-dev.tar.gz -C /

# Use the repo (clone or copy from Windows)
# If the repo is at C:\Users\...\PS5investigations\kstuff-repo on Windows, in WSL it is:
#   /mnt/c/Users/YOUR_USERNAME/.../PS5investigations/kstuff-repo
# Example (adjust path to where you cloned/copied the repo):
cd /mnt/c/Users/YourWindowsUser/Downloads/PS5investigations/kstuff-repo

# Or clone fresh (this repo):
# git clone --recursive https://github.com/Lithiumwow/kstuff.git kstuff-repo && cd kstuff-repo
```

### 3. Build

```bash
# From kstuff-repo root (path from step 2)
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk

# Submodules (BearSSL etc.)
git submodule update --init --recursive

# Lib first (must be built on x86_64 Linux)
cd lib
make clean
make
cd ..

# Payload then loader
cd ps5-kstuff && make && cd ..
cd ps5-kstuff-ldr && make && cd ..
```

### 4. Output

- **`ps5-kstuff-ldr/kstuff.elf`** — use this on the PS5 (send via nc or your loader).

To copy back to Windows:

- In WSL the file is at e.g.  
  `/mnt/c/Users/.../PS5investigations/kstuff-repo/ps5-kstuff-ldr/kstuff.elf`  
  so it’s already visible in File Explorer under the same path.

---

## Option B: GitHub Actions (no local build)

1. Push this repo to GitHub (your fork).
2. Open the repo → **Actions** → workflow **“Rebuild With New SDK”** → **Run workflow** (or it runs on push).
3. When the run finishes → open the run → **Artifacts** → download **Payload** (contains **kstuff.elf**).
4. Use that **kstuff.elf** on the PS5.

---

## Summary

| Where you build | How |
|-----------------|-----|
| **Windows**     | WSL2 + Ubuntu, then follow “Option A” above. |
| **Linux x86_64** | Same steps as in Option A (install deps, SDK, then lib → ps5-kstuff → ps5-kstuff-ldr). |
| **No local build** | Use Option B (GitHub Actions) and download the Payload artifact. |

The resulting **kstuff.elf** includes SquashFS + “drop .sqfs on USB” support. Put `.sqfs` files in the root of USB and keep **unsquashfs.elf** in `/data/` on the PS5.
