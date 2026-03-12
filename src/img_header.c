/**
 * @file img_header.c
 * @brief Implements reading of IMAGEWTY image headers and file headers.
 */

#include "img_header.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Read a 32-bit unsigned integer from a file in little-endian format.
 *
 * @param f   File pointer.
 * @param out Pointer to store the result.
 * @return 0 on success, -1 on read failure.
 */
static int read_uint32_le(FILE* f, uint32_t* out)
{
    uint8_t buf[4];
    if (fread(buf, 1, 4, f) != 4)
    {
        return -1;
    }
    *out = ((uint32_t)buf[0]) | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
    return 0;
}

/**
 * @brief Read the global IMAGEWTY header from the image file.
 *
 * @param f   File pointer to IMAGEWTY file.
 * @param hdr Pointer to ImageWTYHeader structure to populate.
 * @return 0 on success, -1 on read failure.
 */
int read_image_header(FILE* f, ImageWTYHeader* hdr)
{
    if (fread(hdr->magic, 1, 8, f) != 8)
    {
        fprintf(stderr, "Failed to read IMAGEWTY magic\n");
        return -1;
    }
    hdr->magic[8] = '\0'; /* Null-terminate magic string */

    if (read_uint32_le(f, &hdr->header_version) != 0 || read_uint32_le(f, &hdr->header_size) != 0 ||
        read_uint32_le(f, &hdr->base_ram) != 0 || read_uint32_le(f, &hdr->format_version) != 0 ||
        read_uint32_le(f, &hdr->total_image_size) != 0 ||
        read_uint32_le(f, &hdr->header_size_aligned) != 0 ||
        read_uint32_le(f, &hdr->file_header_length) != 0 ||
        read_uint32_le(f, &hdr->usb_product_id) != 0 ||
        read_uint32_le(f, &hdr->usb_vendor_id) != 0 || read_uint32_le(f, &hdr->hardware_id) != 0 ||
        read_uint32_le(f, &hdr->firmware_id) != 0 || read_uint32_le(f, &hdr->unknown1) != 0 ||
        read_uint32_le(f, &hdr->unknown2) != 0 || read_uint32_le(f, &hdr->num_files) != 0 ||
        read_uint32_le(f, &hdr->unknown3) != 0)
    {
        fprintf(stderr, "Failed to read IMAGEWTY header fields\n");
        return -1;
    }

    return 0;
}

/**
 * @brief Read a single file header from the image file.
 *
 * @param f                  File pointer to IMAGEWTY file.
 * @param fh                 Pointer to ImageWTYFileHeader structure to populate.
 * @param file_header_length Length of each file header in bytes.
 * @return 0 on success, -1 on read failure.
 */
int read_file_header(FILE* f, ImageWTYFileHeader* fh, uint32_t file_header_length)
{
    long start_offset = ftell(f);

    /* Basic header fields */
    if (read_uint32_le(f, &fh->filename_length) != 0 || read_uint32_le(f, &fh->header_size) != 0)
    {
        fprintf(stderr, "Failed to read file header basic fields\n");
        return -1;
    }

    /* Read maintype (8 bytes) */
    if (fread(fh->maintype, 1, 8, f) != 8)
    {
        fprintf(stderr, "Failed to read maintype\n");
        return -1;
    }
    fh->maintype[8] = '\0';

    /* Read subtype (16 bytes) */
    if (fread(fh->subtype, 1, 16, f) != 16)
    {
        fprintf(stderr, "Failed to read subtype\n");
        return -1;
    }
    fh->subtype[16] = '\0';

    if (read_uint32_le(f, &fh->unknown0) != 0)
    {
        fprintf(stderr, "Failed to read file header unknown0\n");
        return -1;
    }

    /* Read filename (up to 256 bytes) */
    if (fh->filename_length > 256)
        fh->filename_length = 256;
    if (fread(fh->filename, 1, fh->filename_length, f) != fh->filename_length)
    {
        fprintf(stderr, "Failed to read filename\n");
        return -1;
    }
    fh->filename[fh->filename_length] = '\0';

    /* Skip remaining filename padding */
    fseek(f, 256 - fh->filename_length, SEEK_CUR);

    /* File size fields */
    if (read_uint32_le(f, &fh->stored_length) != 0 || read_uint32_le(f, &fh->pad1) != 0 ||
        read_uint32_le(f, &fh->original_length) != 0 || read_uint32_le(f, &fh->pad2) != 0 ||
        read_uint32_le(f, &fh->offset) != 0)
    {
        fprintf(stderr, "Failed to read file header size fields\n");
        return -1;
    }

    /* Skip any remaining padding in the file header */
    long consumed = ftell(f) - start_offset;
    long skip = (long)file_header_length - consumed;
    if (skip > 0)
        fseek(f, skip, SEEK_CUR);

    return 0;
}

/**
 * @brief Read all file headers from the image.
 *
 * @param f               File pointer to IMAGEWTY file.
 * @param num_files       Number of files in the image.
 * @param file_header_length Length of each file header in bytes.
 * @return Pointer to allocated array of ImageWTYFileHeader structures, or NULL on error.
 */
ImageWTYFileHeader* read_all_file_headers(FILE* f, uint32_t num_files, uint32_t file_header_length)
{
    if (num_files > MAX_IMAGE_FILES)
    {
        fprintf(stderr, "Error: num_files=%u exceeds maximum (%d). File may be corrupted.\n",
                num_files, MAX_IMAGE_FILES);
        return NULL;
    }

    ImageWTYFileHeader* files = malloc(sizeof(ImageWTYFileHeader) * num_files);
    if (!files)
    {
        perror("Failed to allocate memory for file headers");
        return NULL;
    }

    for (uint32_t i = 0; i < num_files; i++)
    {
        if (fseek(f, FILE_HEADERS_START + i * file_header_length, SEEK_SET) != 0)
        {
            perror("Failed to seek to file header");
            free(files);
            return NULL;
        }
        if (read_file_header(f, &files[i], file_header_length) != 0)
        {
            fprintf(stderr, "Failed to read file header %u\n", i);
            free(files);
            return NULL;
        }
    }

    return files;
}
