/**
 * @file checksum.c
 * @brief Functions for computing checksums of files and verifying/updating
 *        "V*.fex" files in a dump folder. Useful to ensure integrity before
 *        repacking firmware images or after extraction.
 */

#include "checksum.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Compute a simple 32-bit checksum for a given file.
 *
 * The checksum sums all 4-byte words in little-endian order.
 * Any remaining bytes (<4) are padded with zeros.
 *
 * @param filename Path to the file.
 * @return 32-bit checksum. Returns 0 if file cannot be opened.
 */
uint32_t compute_checksum(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        fprintf(stderr, "compute_checksum: Cannot open file '%s'\n", filename);
        return 0;
    }

    uint32_t sum = 0;
    uint8_t buf[0x10000]; /* 64 KB buffer */
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        size_t i = 0;

        /* Process all full 4-byte words */
        for (; i + 4 <= n; i += 4)
        {
            uint32_t word = (uint32_t)buf[i] | ((uint32_t)buf[i + 1] << 8) |
                            ((uint32_t)buf[i + 2] << 16) | ((uint32_t)buf[i + 3] << 24);
            sum += word;
        }

        /* Handle remaining bytes, if any */
        size_t rem = n % 4;
        if (rem)
        {
            uint8_t last[4] = {0};
            memcpy(last, buf + n - rem, rem);
            uint32_t word = (uint32_t)last[0] | ((uint32_t)last[1] << 8) |
                            ((uint32_t)last[2] << 16) | ((uint32_t)last[3] << 24);
            sum += word;
        }

        sum &= 0xFFFFFFFFU;
    }

    fclose(f);
    return sum;
}

/**
 * @brief Internal function to verify or update "V*.fex" files in a folder.
 *
 * Can operate in either verification mode or update mode.
 *
 * @param dump_folder Path to the folder containing extracted files.
 * @param update      If non-zero, mismatched checksums are corrected.
 */
static void check_vfiles_common(const char* dump_folder, int update)
{
    if (!dump_folder)
    {
        fprintf(stderr, "check_vfiles_common: dump_folder is NULL\n");
        return;
    }

    DIR* dir = opendir(dump_folder);
    if (!dir)
    {
        fprintf(stderr, "Failed to open directory '%s': %s\n", dump_folder, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        const char* name = entry->d_name;
        size_t name_len = strlen(name);

        /* Only process files starting with 'V' and ending with ".fex" */
        if (name[0] != 'V' || name_len < 4 || strcmp(name + name_len - 4, ".fex") != 0)
            continue;

        /* Skip known vbmeta variants */
        if (strcmp(name, "Vvbmeta.fex") == 0 || strcmp(name, "Vvbmeta_system.fex") == 0 ||
            strcmp(name, "Vvbmeta_vendor.fex") == 0)
        {
            continue;
        }

        /* Build full path to the V*.fex file */
        char vfile_path[1024];
        snprintf(vfile_path, sizeof(vfile_path), "%s/%s", dump_folder, name);

        /* Read stored checksum from V*.fex (first 4 bytes) */
        FILE* vf = fopen(vfile_path, "rb");
        if (!vf)
        {
            fprintf(stderr, "Cannot open '%s'\n", vfile_path);
            continue;
        }

        uint8_t chkbuf[4];
        if (fread(chkbuf, 1, 4, vf) != 4)
        {
            fprintf(stderr, "Failed to read checksum from '%s'\n", vfile_path);
            fclose(vf);
            continue;
        }
        fclose(vf);

        uint32_t expected = (uint32_t)chkbuf[0] | ((uint32_t)chkbuf[1] << 8) |
                            ((uint32_t)chkbuf[2] << 16) | ((uint32_t)chkbuf[3] << 24);

        /* Build path to the real file (remove leading 'V') */
        const char* realname = name + 1;
        char realfile_path[1024];
        snprintf(realfile_path, sizeof(realfile_path), "%s/%s", dump_folder, realname);

        /* Compute actual checksum */
        uint32_t actual = compute_checksum(realfile_path);

        if (actual == expected)
        {
            printf("[OK]   %s checksum matches (%u)\n", realname, actual);
        }
        else
        {
            if (update)
            {
                printf("[FIX]  %s checksum mismatch: expected %u, got %u -> updating...\n",
                       realname, expected, actual);

                FILE* vf2 = fopen(vfile_path, "wb");
                if (!vf2)
                {
                    fprintf(stderr, "Cannot write '%s': %s\n", vfile_path, strerror(errno));
                    continue;
                }

                uint8_t newbuf[4] = {(uint8_t)(actual & 0xFF), (uint8_t)((actual >> 8) & 0xFF),
                                     (uint8_t)((actual >> 16) & 0xFF),
                                     (uint8_t)((actual >> 24) & 0xFF)};

                if (fwrite(newbuf, 1, 4, vf2) != 4)
                {
                    fprintf(stderr, "Error writing checksum to '%s'\n", vfile_path);
                }
                fclose(vf2);
                printf("       Updated checksum in %s to %u\n", name, actual);
            }
            else
            {
                printf("[FAIL] %s checksum mismatch: expected %u, got %u\n", realname, expected,
                       actual);
            }
        }
    }

    closedir(dir);
}

/**
 * @brief Verify checksums of V*.fex files without updating them.
 *
 * @param dump_folder Path to folder containing extracted files.
 */
void verify_vfiles_checksums(const char* dump_folder)
{
    check_vfiles_common(dump_folder, 0);
}

/**
 * @brief Verify and update V*.fex checksums if mismatched.
 *
 * @param dump_folder Path to folder containing extracted files.
 */
void update_vfiles_if_needed(const char* dump_folder)
{
    check_vfiles_common(dump_folder, 1);
}
