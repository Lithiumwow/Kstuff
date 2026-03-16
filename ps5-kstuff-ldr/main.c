/* Copyright (C) 2025 John Törnblom

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <elf.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/_iovec.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <machine/param.h>
#include <ps5/payload.h>
#include <ps5/klog.h>
#include "payload_bin.c"

int patch_app_db(void);
int sceKernelSetProcessName(const char *name);

#define ROUND_PG(x) (((x) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))
#define TRUNC_PG(x) ((x) & ~(PAGE_SIZE - 1))
#define PFLAGS(x)   ((((x) & PF_R) ? PROT_READ  : 0) | \
		     (((x) & PF_W) ? PROT_WRITE : 0) | \
		     (((x) & PF_X) ? PROT_EXEC  : 0))

#define IOVEC_ENTRY(x) { (void*)(x), (x) ? strlen(x) + 1 : 0 }
#define IOVEC_SIZE(x)  (sizeof(x) / sizeof(struct iovec))

static int remount_system_ex(void) {
    struct iovec iov[] = {
        IOVEC_ENTRY("from"),      IOVEC_ENTRY("/dev/ssd0.system_ex"),
        IOVEC_ENTRY("fspath"),    IOVEC_ENTRY("/system_ex"),
        IOVEC_ENTRY("fstype"),    IOVEC_ENTRY("exfatfs"),
        IOVEC_ENTRY("large"),     IOVEC_ENTRY("yes"),
        IOVEC_ENTRY("timezone"),  IOVEC_ENTRY("static"),
        IOVEC_ENTRY("async"),     IOVEC_ENTRY(NULL),
        IOVEC_ENTRY("ignoreacl"), IOVEC_ENTRY(NULL),
    };
    return nmount(iov, IOVEC_SIZE(iov), MNT_UPDATE);
}

static int mount_nullfs(const char* src, const char* dst) {
    struct iovec iov[] = {
        IOVEC_ENTRY("fstype"), IOVEC_ENTRY("nullfs"),
        IOVEC_ENTRY("from"),   IOVEC_ENTRY(src),
        IOVEC_ENTRY("fspath"), IOVEC_ENTRY(dst),
    };
    return nmount(iov, IOVEC_SIZE(iov), 0);
}

/* Check if path ends with .sqfs, .squashfs, or .sqsh (case-insensitive) */
static int is_squashfs_path(const char *path) {
    size_t len = strlen(path);
    if (len >= 5 && strcasecmp(path + len - 5, ".sqfs") == 0) return 1;
    if (len >= 9 && strcasecmp(path + len - 9, ".squashfs") == 0) return 1;
    if (len >= 5 && strcasecmp(path + len - 5, ".sqsh") == 0) return 1;
    return 0;
}

/* Extract SquashFS via unsquashfs.elf if available. Returns 0 on success. */
static int extract_squashfs(const char *sqfs_path, const char *dest_dir) {
    static const char *unsquashfs_paths[] = {
        "/data/unsquashfs.elf",
        "/mnt/usb0/unsquashfs.elf",
        "/mnt/usb1/unsquashfs.elf",
        NULL
    };
    const char *unsquashfs = NULL;
    for (int i = 0; unsquashfs_paths[i]; i++) {
        if (access(unsquashfs_paths[i], X_OK) == 0) {
            unsquashfs = unsquashfs_paths[i];
            break;
        }
    }
    if (!unsquashfs) {
        klog_printf("SquashFS: unsquashfs.elf not found. Put it in /data/ or /mnt/usb0/\n");
        klog_printf("  Or pre-extract on PC and use directory path in mount.lnk\n");
        return -1;
    }
    /* Ensure parent /data/tmp exists */
    if (strncmp(dest_dir, "/data/tmp/", 10) == 0) {
        mkdir("/data/tmp", 0755);
    }
    if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
        klog_perror("SquashFS: failed to create extract dir");
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        klog_perror("SquashFS: fork failed");
        return -1;
    }
    if (pid == 0) {
        char *argv[] = { (char *)unsquashfs, "-f", "-d", (char *)dest_dir, (char *)sqfs_path, NULL };
        execv(unsquashfs, argv);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        klog_printf("SquashFS: unsquashfs failed (exit %d)\n", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return -1;
    }
    return 0;
}

static int bind_mount_title(const char* title_id, const char* src) {
    char dst[PATH_MAX];
    char extract_dir[PATH_MAX];
    const char *mount_src = src;
    struct stat st;

    snprintf(dst, sizeof(dst), "/system_ex/app/%s/sce_sys", title_id);
    if (stat(dst, &st) == 0) {
        klog_printf("Title already mounted: %s\n", title_id);
        return 0;
    }

    /* If src is a SquashFS file, extract first then bind mount the extracted dir */
    if (stat(src, &st) == 0 && S_ISREG(st.st_mode) && is_squashfs_path(src)) {
        snprintf(extract_dir, sizeof(extract_dir), "/data/tmp/%s_sqfs", title_id);
        klog_printf("SquashFS: extracting %s -> %s\n", src, extract_dir);
        if (extract_squashfs(src, extract_dir) != 0) {
            return -1;
        }
        mount_src = extract_dir;
    }

    snprintf(dst, sizeof(dst), "/system_ex/app/%s", title_id);
    if (unmount(dst, 0) != 0 && errno != EINVAL) {
        klog_perror("Failed to unmount partially mounted title");
    }

    if (mkdir(dst, 0755) && errno != EEXIST) {
        klog_perror("Failed to create mount directory for title");
        return -1;
    }

    if (mount_nullfs(mount_src, dst) != 0) {
        klog_perror("Failed to bind mount title with mount_nullfs");
        return -1;
    }

    klog_printf("Title Mounted Successfully: %s -> %s\n", mount_src, dst);
    return 0;
}

static int read_mount_link(const char* path, char* buf, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        klog_perror("Failed to open mount.lnk file");
        return -1;
    }

    memset(buf, 0, size);
    ssize_t n = read(fd, buf, size - 1);
    if (n < 0) {
        klog_perror("Failed to read mount.lnk file");
        close(fd);
        return -1;
    }
    close(fd);
    /* Trim trailing whitespace/newline */
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) {
        buf[--n] = '\0';
    }
    return 0;
}

static int bind_mount_all_titles(const char* path) {
    char mountlnk[PATH_MAX];
    struct dirent *entry;
    struct stat st;
    DIR *dir = opendir(path);

    if (!dir) {
        klog_perror("Failed to open directory while binding mounts");
        return -1;
    }

    while ((entry = readdir(dir))) {
        if (strlen(entry->d_name) != 9) {
            continue;
        }

        snprintf(mountlnk, sizeof(mountlnk), "%s/%s/mount.lnk", path, entry->d_name);

        if (stat(mountlnk, &st) != 0) {
            continue;
        }

        if (read_mount_link(mountlnk, mountlnk, sizeof(mountlnk)) != 0) {
            klog_printf("Failed to read mount.lnk for title %s\n", entry->d_name);
            continue;
        }

        if (bind_mount_title(entry->d_name, mountlnk) != 0) {
            klog_printf("Failed to bind mount title %s -> %s\n", entry->d_name, mountlnk);
            continue;
        }

        klog_printf("Successfully mounted title %s -> %s\n", entry->d_name, mountlnk);
    }

    closedir(dir);
    return 0;
}

/* Scan a directory for .sqfs files and bind-mount each (extract then nullfs). */
static int bind_mount_squashfs_in_dir(const char *dir_path) {
    struct dirent *entry;
    DIR *dir;
    char full_path[PATH_MAX];
    char title_id[10];
    const char *base;
    size_t len, i;
    struct stat st;

    dir = opendir(dir_path);
    if (!dir)
        return 0;
    while ((entry = readdir(dir))) {
        if (!is_squashfs_path(entry->d_name))
            continue;
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        base = entry->d_name;
        len = strlen(base);
        if (len >= 5 && strcasecmp(base + len - 5, ".sqfs") == 0)
            len -= 5;
        else if (len >= 9 && strcasecmp(base + len - 9, ".squashfs") == 0)
            len -= 9;
        else if (len >= 5 && strcasecmp(base + len - 5, ".sqsh") == 0)
            len -= 5;
        else
            continue;
        for (i = 0; i < 9 && i < len; i++)
            title_id[i] = base[i];
        title_id[i] = '\0';
        if (i == 0)
            continue;
        if (bind_mount_title(title_id, full_path) == 0)
            klog_printf("SquashFS from USB: %s -> /system_ex/app/%s\n", full_path, title_id);
    }
    closedir(dir);
    return 0;
}

/* Scan USB mounts for .sqfs and mount them (add .sqfs to USB = auto mount). */
static void bind_mount_squashfs_on_usb(void) {
    static const char *usb_paths[] = { "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", NULL };
    for (int i = 0; usb_paths[i]; i++) {
        if (access(usb_paths[i], F_OK) == 0)
            bind_mount_squashfs_in_dir(usb_paths[i]);
    }
}

static int monitor_usb_changes(void) {
    struct kevent evt;
    int kq;

    if ((kq = kqueue()) < 0) {
        klog_perror("Failed to create kqueue");
        return -1;
    }

    EV_SET(&evt, 0, EVFILT_FS, EV_ADD | EV_CLEAR, 0, 0, 0);
    if (kevent(kq, &evt, 1, NULL, 0, NULL) < 0) {
        klog_perror("Failed to register usb event filter with kevent");
        close(kq);
        return -1;
    }

    while (1) {
        if (kevent(kq, NULL, 0, &evt, 1, NULL) < 0) {
            klog_perror("kevent wait failed while monitoring USB changes");
            break;
        }

        if (bind_mount_all_titles("/user/app") < 0) {
            klog_perror("Failed to bind mount /user/app titles after USB change");
        }
        bind_mount_squashfs_on_usb();
    }

    close(kq);
    return 0;
}

static void
pt_load(const void* image, void* base, Elf64_Phdr *phdr) {
  if(phdr->p_memsz && phdr->p_filesz) {
      memcpy(base + phdr->p_vaddr, image + phdr->p_offset, phdr->p_filesz);
  }
}

int main(void) {
	sceKernelSetProcessName("kstuff.elf");
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)___ps5_kstuff_payload_bin;
    Elf64_Phdr *phdr = (Elf64_Phdr*)(___ps5_kstuff_payload_bin + ehdr->e_phoff);
    Elf64_Shdr *shdr = (Elf64_Shdr*)(___ps5_kstuff_payload_bin + ehdr->e_shoff);
    void *base = (void*)0x0000000926100000;
    uintptr_t min_vaddr = -1;
    uintptr_t max_vaddr = 0;
    size_t base_size;

    // Compute size of virtual memory region.
    for(int i=0; i<ehdr->e_phnum; i++) {
        if(phdr[i].p_vaddr < min_vaddr) {
            min_vaddr = phdr[i].p_vaddr;
        }

        if(max_vaddr < phdr[i].p_vaddr + phdr[i].p_memsz) {
            max_vaddr = phdr[i].p_vaddr + phdr[i].p_memsz;
        }
    }
    min_vaddr = TRUNC_PG(min_vaddr);
    max_vaddr = ROUND_PG(max_vaddr);
    base_size = max_vaddr - min_vaddr;

    // allocate memory.
    if((base=mmap(base, base_size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    // Parse program headers.
    for(int i=0; i<ehdr->e_phnum; i++) {
        switch(phdr[i].p_type) {
        case PT_LOAD:
            pt_load(___ps5_kstuff_payload_bin, base, &phdr[i]);
            break;
        }
    }

    // Set protection bits on mapped segments.
    for(int i=0; i<ehdr->e_phnum; i++) {
        if(phdr[i].p_type != PT_LOAD || phdr[i].p_memsz == 0) {
            continue;
        }
        if(mprotect(base + phdr[i].p_vaddr, ROUND_PG(phdr[i].p_memsz),
                    PFLAGS(phdr[i].p_flags))) {
            perror("mprotect");
            return EXIT_FAILURE;
        }
    }

    void (*entry)(payload_args_t*) = base + ehdr->e_entry;
    payload_args_t* args = payload_get_args();

    entry(args);
    if(*args->payloadout == 0) {
        puts("patching app.db");
        *args->payloadout = patch_app_db();
    }

    klog_printf("Remounting /system_ex and mounting titles...\n");
    remount_system_ex();
    bind_mount_all_titles("/user/app");
    bind_mount_squashfs_on_usb();

    monitor_usb_changes();

    return 0; 
}
