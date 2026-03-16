# Kstuff (SquashFS + USB support)

PS5 kernel payload with **SquashFS** and **USB .sqfs auto-mount**: put `.sqfs` files on USB and they are extracted and bind-mounted (no `/user/app` setup).

## Build on Windows

Use **WSL2 + Ubuntu**, then follow **[BUILD_KSTUFF_WINDOWS.md](BUILD_KSTUFF_WINDOWS.md)**.

Quick clone and build (in WSL Ubuntu):

```bash
git clone --recursive https://github.com/Lithiumwow/kstuff.git
cd kstuff
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk   # after installing SDK (see doc)
cd lib && make clean && make && cd ..
cd ps5-kstuff && make && cd ..
cd ps5-kstuff-ldr && make && cd ..
# → ps5-kstuff-ldr/kstuff.elf
```

## Build on Linux / CI

- **Linux x86_64:** See [BUILD_KSTUFF.md](BUILD_KSTUFF.md).
- **GitHub Actions:** Push to this repo → **Actions** → “Rebuild With New SDK” → download **Payload** artifact for `kstuff.elf`.

## Usage on PS5

- Load **kstuff.elf** (e.g. `nc PS5_IP 9021 < kstuff.elf`).
- Put **unsquashfs.elf** in `/data/` (or `/mnt/usb0/`).
- Put `.sqfs` files in the **root** of USB (e.g. `PPSA17221-app.sqfs`); replug or reload payload.
