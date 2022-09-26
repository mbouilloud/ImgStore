/* ** NOTE: undocumented in Doxygen
 * @file tools.c
 * @brief implementation of several tool functions for imgStore
 *
 * @author Mia Primorac
 */

#include "imgStore.h"
#include "util.h"

#include <stdint.h> // for uint8_t
#include <stdio.h> // for sprintf
#include <stdlib.h>
#include <openssl/sha.h> // for SHA256_DIGEST_LENGTH

#define SHA256_STRING_LENGTH (2*SHA256_DIGEST_LENGTH) // length of a human-readable SHA

/********************************************************************//**
 * Human-readable SHA
 */
static void
sha_to_string (const unsigned char* SHA,
               char* sha_string)
{
    if (SHA == NULL) {
        return;
    }
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(&sha_string[i*2], "%02x", SHA[i]);
    }

    sha_string[2*SHA256_DIGEST_LENGTH] = '\0';
}


/********************************************************************//**
 * imgStore header display.
 */
// See imgStore.h
void
print_header (const struct imgst_header * header)
{
    if (header != NULL) {
        printf("*****************************************\n");
        printf("**********IMGSTORE HEADER START**********\n");
        printf("TYPE: %31s\n", header->imgst_name);
        printf("VERSION: %" PRIu32"\n", header->imgst_version);
        printf("IMAGE COUNT: %" PRIu32"\t\tMAX IMAGES: %" PRIu32"\n", header->num_files, header->max_files);
        printf("THUMBNAIL: %" PRIu16" x %"PRIu16"\tSMALL: %"PRIu16" x %"PRIu16"\n",
               header->res_resized[0], header->res_resized[1], header->res_resized[2], header->res_resized[3]);
        printf("***********IMGSTORE HEADER END***********\n");
        printf("*****************************************\n");
    }
}

/********************************************************************//**
 * Metadata display.
 */
// See imgStore.h
void
print_metadata (const struct img_metadata * metadata)
{
    if (metadata != NULL) {
        char sha_printable[2*SHA256_DIGEST_LENGTH+1];
        sha_to_string(metadata->SHA, sha_printable);

        printf("IMAGE ID: %s\n", metadata->img_id);
        printf("SHA: %s\n", sha_printable);
        printf("VALID: %"PRIu16"\n", metadata->is_valid);
        printf("UNUSED: %"PRIu16"\n", metadata->unused_16);
        printf("OFFSET ORIG. : %" PRIu64"\t SIZE ORIG. : %" PRIu32"\n", metadata->offset[RES_ORIG], metadata->size[RES_ORIG]);
        printf("OFFSET THUMB.: %" PRIu64"\t SIZE THUMB.: %" PRIu32"\n", metadata->offset[RES_THUMB], metadata->size[RES_THUMB]);
        printf("OFFSET SMALL : %" PRIu64"\t SIZE SMALL : %" PRIu32"\n", metadata->offset[RES_SMALL], metadata->size[RES_SMALL]);
        printf("ORIGINAL: %" PRIu32" x %" PRIu32"\n", metadata->res_orig[0], metadata->res_orig[1]);
        printf("*****************************************\n");
    }
}

// See imgStore.h
int
do_open (const char* imgst_filename, const char* open_mode, struct imgst_file* imgst_file)
{
    M_REQUIRE_NON_NULL(imgst_filename);
    M_REQUIRE_NON_NULL(open_mode);
    M_REQUIRE_NON_NULL(imgst_file);

    // Opens the binary file on which has been written the DB
    imgst_file->file = fopen(imgst_filename, open_mode);
    if (NULL == imgst_file->file) return ERR_IO;

    // Reads the header from the file
    size_t nb_ok = fread(&imgst_file->header,  sizeof(struct imgst_header), 1, imgst_file->file);
    if (nb_ok != 1 || imgst_file->header.max_files > MAX_MAX_FILES) {
        CLOSE_FILE(imgst_file->file);
        return ERR_IO;
    }

    // Allocates the metadata on the heap
    struct img_metadata * metadata = calloc(imgst_file->header.max_files, sizeof(struct img_metadata));
    if (NULL == metadata) {
        CLOSE_FILE(imgst_file->file);
        return ERR_OUT_OF_MEMORY;
    }
    imgst_file->metadata = metadata;

    // Reads the metadata from the file
    nb_ok = fread(imgst_file->metadata, sizeof(struct img_metadata), imgst_file->header.max_files, imgst_file->file);
    if (nb_ok != imgst_file->header.max_files) {
        CLOSE_FILE(imgst_file->file);
        FREE_POINTER(imgst_file->metadata);
        return ERR_IO;
    }
    return ERR_NONE;
}

// See imgStore.h
void
do_close (struct imgst_file* imgst_file)
{
    if (imgst_file != NULL) {
        if (imgst_file->file != NULL) {
            CLOSE_FILE(imgst_file->file);
        }
        if (imgst_file->metadata != NULL) {
            FREE_POINTER(imgst_file->metadata);
        }
    }
}

// See imgStore.h
int
resolution_atoi (const char* resolution)
{
    M_REQUIRE_NON_NULL_CUSTOM_ERR(resolution, -1);

    if (!strcmp(resolution, "thumb") || !strcmp(resolution, "thumbnail")) return RES_THUMB;
    if (!strcmp(resolution, "small"))                                     return RES_SMALL;
    if (!strcmp(resolution, "orig") || !strcmp(resolution, "original"))   return RES_ORIG;

    return -1;
}

// See imgStore.h
int
update_metadata (struct imgst_file * imgst_file, const size_t index)
{
    M_REQUIRE_NON_NULL(imgst_file);
    M_REQUIRE(index >= 0, ERR_INVALID_ARGUMENT, "input value (%lu) is too low (< 0)", i);

    // Sets the file position indicator to the position of the metadata of the image
    int offset = sizeof(struct imgst_header) + index * sizeof(struct img_metadata);
    fseek(imgst_file->file, offset, SEEK_SET);

    // Writes the updated metadata on the disk at the offset position
    if (fwrite(&(imgst_file->metadata[index]), sizeof(struct img_metadata), 1, imgst_file->file) != 1) return ERR_IO;

    return ERR_NONE;
}

// See imgStore.h
int
update_header (struct imgst_file * imgst_file)
{
    M_REQUIRE_NON_NULL(imgst_file);

    // Writes the content of the header at the beginning of the file
    rewind(imgst_file->file);
    if (fwrite(&(imgst_file->header), sizeof(struct imgst_header), 1, imgst_file->file) != 1) return ERR_IO;

    return ERR_NONE;
}

// See imgStore.h
int
compare_sha (const unsigned char* sha1, const unsigned char* sha2)
{
    M_REQUIRE_NON_NULL(sha1);
    M_REQUIRE_NON_NULL(sha2);

    // Transforms the first SHA value into a string
    char* sha1_str;
    M_EXIT_IF_NULL(sha1_str = malloc(SHA256_STRING_LENGTH+1), SHA256_STRING_LENGTH+1);
    sha_to_string(sha1, sha1_str);

    // Transforms the second SHA value into a string
    char* sha2_str = malloc(SHA256_STRING_LENGTH+1);
    if (sha2_str == NULL) {
        FREE_POINTER(sha1_str);
        return ERR_OUT_OF_MEMORY;
    }
    sha_to_string(sha2, sha2_str);

    // 0 if and only if the two strings are equal
    int are_equal = strncmp(sha1_str, sha2_str, SHA256_STRING_LENGTH);

    // Clean-up
    FREE_POINTER(sha1_str);
    FREE_POINTER(sha2_str);

    return are_equal;
}

// See imgStore.h
int
write_image_end_of_imgst (size_t index, const int res, const char* buffer, size_t size, struct imgst_file* imgst_file)
{
    // Sets the file position indicator to the end of the imgStore, and sets the memory position of the image
    fseek(imgst_file->file, 0, SEEK_END);
    int offset = ftell(imgst_file->file);

    // Writes the buffer content (i.e. the image) and the image position to the imgStore
    if (fwrite(buffer, size, 1, imgst_file->file) != 1) return ERR_IO;
    imgst_file->metadata[index].offset[res] = offset;

    return ERR_NONE;
}

// See imgStore.h
int
load_image_from_imgst (size_t index, const int resolution, char* image_buffer, uint32_t image_size, struct imgst_file* imgst_file)
{
    // Sets the file position indicator to the position of the image in memory
    fseek(imgst_file->file, imgst_file->metadata[index].offset[resolution], SEEK_SET);

    // Loads the image in the buffer
    if (fread(image_buffer, image_size, 1, imgst_file->file) != 1) return ERR_IO;

    return ERR_NONE;
}