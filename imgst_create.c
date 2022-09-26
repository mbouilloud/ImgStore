/**
 * @file imgst_create.c
 * @brief imgStore library: do_create implementation.
 */

#include "imgStore.h"
#include "util.h"

#include <stdio.h>
#include <string.h> // for strncpy
#include <stdlib.h> // for calloc

// See imgStore.h
int do_create (const char* imgst_filename, struct imgst_file * imgst_file)
{
    M_REQUIRE_NON_NULL(imgst_filename);
    M_REQUIRE_NON_NULL(imgst_file);

    // Sets header fields
    imgst_file->header.imgst_version = 0;
    imgst_file->header.num_files = 0;
    // Sets the DB header name
    strncpy(imgst_file->header.imgst_name, CAT_TXT, MAX_IMGST_NAME);
    imgst_file->header.imgst_name[MAX_IMGST_NAME] = '\0';


    // Creates metadata and sets fields
    struct img_metadata * metadata = calloc(imgst_file->header.max_files, sizeof(struct img_metadata));
    M_EXIT_IF_NULL(metadata, sizeof(struct img_metadata));

    for (size_t i = 0; i < imgst_file->header.max_files; ++i) {
        metadata[i].is_valid = EMPTY;
    }
    imgst_file->metadata = metadata;


    // Creates the binary file on which will be written the DB
    imgst_file->file = fopen(imgst_filename, "wb+");
    if (NULL == imgst_file->file) {
        FREE_POINTER(imgst_file->metadata);
        return ERR_IO;
    }

    // Writes the header and metadata on the file
    size_t nb_ok = fwrite(&imgst_file->header,  sizeof(struct imgst_header),                            1, imgst_file->file);
    nb_ok       += fwrite(imgst_file->metadata, sizeof(struct img_metadata), imgst_file->header.max_files, imgst_file->file);

    printf("%zu item(s) written\n", nb_ok); // number of items effectively written on the disk
    if (nb_ok != 1 + imgst_file->header.max_files) {
        CLOSE_FILE(imgst_file->file);
        FREE_POINTER(imgst_file->metadata);
        return ERR_IO;
    }

    return ERR_NONE;
}
