# Build kstuff.elf (with SquashFS USB support)

**Yes — you can replace your current kstuff.elf with this build and it will work.** This repo has the SquashFS + USB .sqfs auto-mount changes.

## 1. Prerequisites

- **PS5 Payload SDK** (you have it)
- **yasm** (assembler): `brew install yasm` on Mac
- **sqlite3** dev library for the toolchain (SDK may ship it)

```bash
export PS5_PAYLOAD_SDK=$HOME/ps5-payload-sdk   # or your path
```

## 2. Build

From the **kstuff-repo** directory:

```bash
cd /Users/lithiumwow/Downloads/PS5investigations/kstuff-repo

# Build payload (ps5-kstuff) first
make -C ps5-kstuff

# Build loader → produces kstuff.elf
make -C ps5-kstuff-ldr
```

Output: **`ps5-kstuff-ldr/kstuff.elf`**

## 3. Use it

- Load **this** `kstuff.elf` instead of the one you used before (same way: nc, prospero-deploy, or your loader).
- Put `.sqfs` files in the **root** of your USB (e.g. `PPSA17221-app.sqfs`).
- Keep `unsquashfs.elf` in `/data/` on the PS5.
- Replug USB (or load kstuff after USB is in); games should appear.

## If the build fails

- **yasm not found:** `brew install yasm`
- **PS5_PAYLOAD_SDK undefined:** `export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk`
- **sqlite3 link error:** The SDK or sysroot may need a sqlite3 lib; check the SDK docs for building apps that use sqlite3.
