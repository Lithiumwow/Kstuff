# Build kstuff.elf (SquashFS + USB support)

This repo **cannot be built on Mac (Apple Silicon)** — the payload uses x86 asm and expects a Linux x86_64 build. Use one of these:

---

## Option 1: GitHub Actions (recommended)

1. **Push this repo to GitHub** (your fork or a new repo).
   ```bash
   cd /Users/lithiumwow/Downloads/PS5investigations/kstuff-repo
   git remote add origin https://github.com/YOUR_USERNAME/YOUR_REPO.git
   git add -A && git commit -m "SquashFS USB support" && git push -u origin main
   ```
   (Use your branch name if not `main`.)

2. Open the repo on GitHub → **Actions** → workflow **"Rebuild With New SDK"**.

3. Click **Run workflow** (or it runs automatically on push).

4. When the run finishes, open the run → **Artifacts** → download **Payload**. It contains **kstuff.elf**.

5. Use that **kstuff.elf** on the PS5 (replace your current kstuff with it).

---

## Option 2: Build on Linux x86_64

On Ubuntu/Debian x86_64 (PC or VM):

```bash
# Install deps
sudo apt update
sudo apt install -y build-essential xxd yasm python3 make git wget

# Install PS5 SDK (from ps5-payload-dev)
wget https://github.com/ps5-payload-dev/pacbrew-repo/releases/latest/download/ps5-payload-dev.tar.gz
sudo tar xf ps5-payload-dev.tar.gz -C /
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk

# Build
cd /path/to/kstuff-repo
git submodule update --init --recursive
cd ps5-kstuff && make && cd ..
cd ps5-kstuff-ldr && make && cd ..
```

Output: **`ps5-kstuff-ldr/kstuff.elf`**

---

## Summary

| Where you are | How to get kstuff.elf |
|---------------|------------------------|
| Mac (this repo only) | Push to GitHub → Actions → download Payload artifact. |
| Linux x86_64  | Install SDK + deps, run the commands above. |

The **kstuff.elf** you get is the one with SquashFS + “drop .sqfs on USB” support.
