# SquashFS Support in kstuff-ldr

When `mount.lnk` points to a **SquashFS image** (`.sqfs`, `.squashfs`, `.sqsh`), kstuff-ldr will:

1. Extract the image using `unsquashfs.elf`
2. Bind-mount the extracted directory with `nullfs`

The PS5 kernel does not have a SquashFS driver, so direct `nmount(..., "squashfs", ...)` fails with EINVAL. This extract-then-bind approach works around that.

## Requirements

- **unsquashfs.elf** – A PS5-compatible build of `unsquashfs` from squashfs-tools.

Place it in one of:

- `/data/unsquashfs.elf`
- `/mnt/usb0/unsquashfs.elf`
- `/mnt/usb1/unsquashfs.elf`

## Usage

1. Create a SquashFS image on PC:  
   `mksquashfs /path/to/game game.sqfs -comp lz4 -b 1M -Xhc`

2. Put the image on USB and create the app structure, e.g.:
   ```
   /user/app/PPSA17221/mount.lnk  →  contains: /mnt/usb0/PPSA17221-app.sqfs
   ```

3. Load kstuff.elf; it will extract and bind-mount automatically.

## Alternative: Pre-extract on PC

If you don't have `unsquashfs.elf` on the PS5:

1. On PC: `unsquashfs -d /path/to/extracted game.sqfs`
2. Put the extracted folder on USB
3. Set `mount.lnk` to the extracted directory path (e.g. `/mnt/usb0/PPSA17221-app`)

Then kstuff-ldr will bind-mount the directory directly without extraction.

## Building unsquashfs for PS5

Build squashfs-tools with the PS5 Payload SDK (prospero-clang). You may need to adjust the build for Orbis/FreeBSD compatibility.
