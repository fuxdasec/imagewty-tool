/**
 * @file config_file.c
 * @brief Functions to load and write IMAGEWTY configuration files (.cfg).
 *
 * This module parses configuration files that describe Allwinner IMAGEWTY
 * firmware images, including global header fields and per-file metadata.
 */

#include "config_file.h"

#include "img_header.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Utility helpers
 * --------------------------------------------------------------------------*/

/**
 * @brief Strip leading and trailing whitespace from a string (in-place).
 *
 * @param s Input string to modify.
 * @return Pointer to the trimmed string (inside s).
 */
static char* strip_whitespace(char* s)
{
    if (!s)
        return NULL;

    while (*s && isspace((unsigned char)*s))
        s++;

    char* end = s + strlen(s);
    if (end != s)
    {
        end--;
        while (end >= s && isspace((unsigned char)*end))
            *end-- = '\0';
    }

    return s;
}

/**
 * @brief Clean a configuration string.
 *
 * Removes whitespace, trailing semicolons, and optional double quotes.
 *
 * @param s Input string to clean.
 * @return Pointer to the cleaned string (may point inside s).
 */
static char* clean_cfg_string(char* s)
{
    if (!s)
        return NULL;

    s = strip_whitespace(s);
    size_t len = strlen(s);
    if (len == 0)
        return s;

    /* Remove trailing semicolon */
    if (s[len - 1] == ';')
    {
        s[--len] = '\0';
    }

    /* Remove surrounding quotes */
    if (len >= 2 && s[0] == '"' && s[len - 1] == '"')
    {
        s[len - 1] = '\0';
        s++;
    }

    return s;
}

/**
 * @brief Parse a string as a 32-bit unsigned integer.
 *
 * Accepts both decimal and hex (0x) formats.
 *
 * @param str Input string.
 * @return Parsed value or 0 if NULL.
 */
static uint32_t parse_uint32(const char* str)
{
    if (!str)
        return 0;
    return (strncmp(str, "0x", 2) == 0) ? (uint32_t)strtoul(str, NULL, 16)
                                        : (uint32_t)strtoul(str, NULL, 10);
}

/* --------------------------------------------------------------------------
 * Field mapping structures
 * --------------------------------------------------------------------------*/

typedef struct
{
    const char* name;
    uint32_t* field;
} UIntFieldMap;

typedef struct
{
    const char* name;
    char* field;
    size_t size;
} StrFieldMap;

/* --------------------------------------------------------------------------
 * Load IMAGEWTY configuration
 * --------------------------------------------------------------------------*/

/**
 * @brief Load an IMAGEWTY configuration file (.cfg) into memory.
 *
 * @param cfg_path Path to the configuration file.
 * @param hdr      Pointer to ImageWTYHeader to fill.
 * @param files    Output pointer to an array of ImageWTYFileHeader.
 * @return 0 on success, non-zero on failure.
 */
int load_image_config(const char* cfg_path, ImageWTYHeader* hdr, ImageWTYFileHeader** files)
{
    if (!cfg_path || !hdr || !files)
        return 1;

    FILE* f = fopen(cfg_path, "r");
    if (!f)
        return 1;

    memset(hdr, 0, sizeof(*hdr));
    *files = NULL;

    char line[512];
    uint32_t num_files = 0;

    /* Global header field maps */
    UIntFieldMap hdr_uint_fields[] = {
        {"header_version", &hdr->header_version},
        {"header_size", &hdr->header_size},
        {"base_ram", &hdr->base_ram},
        {"format_version", &hdr->format_version},
        {"total_image_size", &hdr->total_image_size},
        {"header_size_including_alignment", &hdr->header_size_aligned},
        {"file_header_length", &hdr->file_header_length},
        {"usb_product_id", &hdr->usb_product_id},
        {"usb_vendor_id", &hdr->usb_vendor_id},
        {"hardware_id", &hdr->hardware_id},
        {"firmware_id", &hdr->firmware_id},
        {"unknown_field_1", &hdr->unknown1},
        {"unknown_field_2", &hdr->unknown2},
        {"unknown_field_3", &hdr->unknown3},
        {NULL, NULL}};
    StrFieldMap hdr_str_fields[] = {{"magic", hdr->magic, sizeof(hdr->magic)}, {NULL, NULL, 0}};

    /* --- Read global header fields --- */
    while (fgets(line, sizeof(line), f))
    {
        char* l = strip_whitespace(line);
        if (!*l || l[0] == '#' || l[0] == ';')
            continue;

        char* eq = strchr(l, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char* key = strip_whitespace(l);
        char* val = clean_cfg_string(eq + 1);
        if (!key || !val)
            continue;

        /* String fields */
        for (StrFieldMap* sf = hdr_str_fields; sf->name; sf++)
        {
            if (strcmp(key, sf->name) == 0)
            {
                snprintf(sf->field, sf->size, "%s", val);
                goto next_line;
            }
        }

        /* UInt32 fields */
        for (UIntFieldMap* uf = hdr_uint_fields; uf->name; uf++)
        {
            if (strcmp(key, uf->name) == 0)
            {
                *uf->field = parse_uint32(val);
                goto next_line;
            }
        }

        if (strcmp(key, "number_of_files") == 0)
            num_files = parse_uint32(val);

    next_line:;
    }

    hdr->num_files = num_files;
    if (num_files == 0)
    {
        fclose(f);
        return 0;
    }

    /* Allocate file header array */
    ImageWTYFileHeader* file_array = calloc(num_files, sizeof(ImageWTYFileHeader));
    if (!file_array)
    {
        fclose(f);
        return 1;
    }

    /* --- Parse file blocks --- */
    rewind(f);
    uint32_t current_file = 0;
    ImageWTYFileHeader* fh = NULL;
    int in_file_block = 0;

    while (fgets(line, sizeof(line), f))
    {
        char* l = strip_whitespace(line);
        if (!*l || l[0] == '#' || l[0] == ';')
            continue;

        /* Start of a file block */
        if (!in_file_block && strncmp(l, "file_", 5) == 0 && strchr(l, '{'))
        {
            if (current_file >= num_files)
                break;
            fh = &file_array[current_file];
            memset(fh, 0, sizeof(*fh));
            in_file_block = 1;
            continue;
        }

        if (in_file_block)
        {
            /* End of file block */
            if (strchr(l, '}'))
            {
                current_file++;
                in_file_block = 0;
                fh = NULL;
                continue;
            }

            char* eq = strchr(l, '=');
            if (!eq)
                continue;

            *eq = '\0';
            char* key = strip_whitespace(l);
            char* val = clean_cfg_string(eq + 1);
            if (!key || !val)
                continue;

            /* Field maps for a single file entry */
            UIntFieldMap file_uint_fields[] = {{"filename_length", &fh->filename_length},
                                               {"file_header_size", &fh->header_size},
                                               {"unknown0", &fh->unknown0},
                                               {"stored_length", &fh->stored_length},
                                               {"pad1", &fh->pad1},
                                               {"original_length", &fh->original_length},
                                               {"pad2", &fh->pad2},
                                               {"offset", &fh->offset},
                                               {NULL, NULL}};
            StrFieldMap file_str_fields[] = {{"maintype", fh->maintype, sizeof(fh->maintype)},
                                             {"subtype", fh->subtype, sizeof(fh->subtype)},
                                             {"filename", fh->filename, sizeof(fh->filename)},
                                             {NULL, NULL, 0}};

            /* String fields */
            for (StrFieldMap* sf = file_str_fields; sf->name; sf++)
            {
                if (strcmp(key, sf->name) == 0)
                {
                    snprintf(sf->field, sf->size, "%s", val);
                    goto next_file_line;
                }
            }

            /* UInt32 fields */
            for (UIntFieldMap* uf = file_uint_fields; uf->name; uf++)
            {
                if (strcmp(key, uf->name) == 0)
                {
                    *uf->field = parse_uint32(val);
                    goto next_file_line;
                }
            }

        next_file_line:;
        }
    }

    fclose(f);
    *files = file_array;
    return 0;
}

/* --------------------------------------------------------------------------
 * Write IMAGEWTY configuration
 * --------------------------------------------------------------------------*/

/**
 * @brief Write an IMAGEWTY configuration file (.cfg).
 *
 * @param cfg_path  Output path for the configuration file.
 * @param hdr       Pointer to ImageWTYHeader.
 * @param files     Pointer to file headers array.
 * @param num_files Number of files in the array.
 * @return 0 on success, non-zero on failure.
 */
int write_image_config(const char* cfg_path, const ImageWTYHeader* hdr,
                       const ImageWTYFileHeader* files, uint32_t num_files)
{
    if (!cfg_path || !hdr)
        return 1;

    FILE* f = fopen(cfg_path, "w");
    if (!f)
        return 1;

    fprintf(f, "[IMAGE_CFG]\n");
    fprintf(f, "magic=\"%s\";\n", hdr->magic);

    UIntFieldMap hdr_uint_fields[] = {
        {"header_version", (uint32_t*)&hdr->header_version},
        {"header_size", (uint32_t*)&hdr->header_size},
        {"base_ram", (uint32_t*)&hdr->base_ram},
        {"format_version", (uint32_t*)&hdr->format_version},
        {"total_image_size", (uint32_t*)&hdr->total_image_size},
        {"header_size_including_alignment", (uint32_t*)&hdr->header_size_aligned},
        {"file_header_length", (uint32_t*)&hdr->file_header_length},
        {"usb_product_id", (uint32_t*)&hdr->usb_product_id},
        {"usb_vendor_id", (uint32_t*)&hdr->usb_vendor_id},
        {"hardware_id", (uint32_t*)&hdr->hardware_id},
        {"firmware_id", (uint32_t*)&hdr->firmware_id},
        {"unknown_field_1", (uint32_t*)&hdr->unknown1},
        {"unknown_field_2", (uint32_t*)&hdr->unknown2},
        {"unknown_field_3", (uint32_t*)&hdr->unknown3},
        {NULL, NULL}};

    for (UIntFieldMap* uf = hdr_uint_fields; uf->name; uf++)
        fprintf(f, "%s=0x%08X;\n", uf->name, *uf->field);

    fprintf(f, "number_of_files=0x%08X;\n", num_files);

    /* Write file blocks */
    if (num_files && files)
    {
        fprintf(f, "\n[FILELIST]\n");
        for (uint32_t i = 0; i < num_files; i++)
        {
            const ImageWTYFileHeader* fh = &files[i];
            fprintf(f, "file_%u {\n", i + 1);
            fprintf(f, "filename_length=0x%08X;\n", fh->filename_length);
            fprintf(f, "file_header_size=0x%08X;\n", fh->header_size);
            fprintf(f, "maintype=\"%s\";\n", fh->maintype);
            fprintf(f, "subtype=\"%s\";\n", fh->subtype);
            fprintf(f, "unknown0=0x%08X;\n", fh->unknown0);
            fprintf(f, "filename=\"%s\";\n", fh->filename);
            fprintf(f, "stored_length=0x%08X;\n", fh->stored_length);
            fprintf(f, "pad1=0x%08X;\n", fh->pad1);
            fprintf(f, "original_length=0x%08X;\n", fh->original_length);
            fprintf(f, "pad2=0x%08X;\n", fh->pad2);
            fprintf(f, "offset=0x%08X;\n", fh->offset);
            fprintf(f, "}\n");
        }
    }

    fclose(f);
    return 0;
}

/* --------------------------------------------------------------------------
 * Free resources
 * --------------------------------------------------------------------------*/

/**
 * @brief Free an allocated file header list.
 *
 * @param files Pointer to ImageWTYFileHeader array.
 */
void free_file_list(ImageWTYFileHeader* files)
{
    free(files);
}
