/**
 * @file imgStore.h
 * @brief Main header file for imgStore core library.
 *
 * Defines the format of the data structures that will be stored on the disk
 * and provides interface functions.
 *
 * The image imgStore starts with exactly one header structure
 * followed by exactly imgst_header.max_files metadata
 * structures. The actual content is not defined by these structures
 * because it should be stored as raw bytes appended at the end of the
 * imgStore file and addressed by offsets in the metadata structure.
 *
 * @author Mia Primorac
 */

#ifndef IMGSTOREPRJ_IMGSTORE_H
#define IMGSTOREPRJ_IMGSTORE_H

#include "error.h" /* not needed in this very file,
                    * but we provide it here, as it is required by
                    * all the functions of this lib.
                    */
#include <stdio.h> // for FILE
#include <stdint.h> // for uint32_t, uint64_t
#include <openssl/sha.h> // for SHA256_DIGEST_LENGTH

#define CAT_TXT "EPFL ImgStore binary"

/* constraints */
#define MAX_IMGST_NAME  31  // max. size of a ImgStore name
#define MAX_IMG_ID     127  // max. size of an image id
#define MAX_MAX_FILES 100000  // will be increased later in the project

/* For is_valid in imgst_metadata */
#define EMPTY 0
#define NON_EMPTY 1

// imgStore library internal codes for different image resolutions.
#define RES_THUMB 0
#define RES_SMALL 1
#define RES_ORIG  2
#define NB_RES    3

#ifdef __cplusplus
extern "C" {
#endif

/* The configuration information of the imgStore database */
struct imgst_header {
    char imgst_name[MAX_IMGST_NAME+1];          // name of the database
    uint32_t imgst_version;                     // version of the database
    uint32_t num_files;                         // number of (valid) images in the database
    const uint32_t max_files;                   // maximal number of images in the database
    const uint16_t res_resized[2 * (NB_RES-1)]; // array of the maximal resolutions of "thumbnail" and "small"
    uint32_t unused_32;
    uint64_t unused_64;
};

/* The metadata of an image */
struct img_metadata {
    char img_id[MAX_IMG_ID+1];               // ID of the image
    unsigned char SHA[SHA256_DIGEST_LENGTH]; // hash code of the image
    uint32_t res_orig[2];                    // resolution of the original image
    uint32_t size[NB_RES];                   // number of bytes of the images in different resolutions
    uint32_t padding0;                       // for padding of the struct
    uint64_t offset[NB_RES];                 // positions in the imgStore of the images in different resolutions
    uint16_t is_valid;                       // whether the image is still in use or has been deleted
    uint16_t unused_16;
    uint32_t padding1;                       // for padding of the struct
};

/* The database */
struct imgst_file {
    FILE* file;                    // database file (on the disk)
    struct imgst_header header;    // header of the database
    struct img_metadata* metadata; // metadata of the images in the database
};

/**
 * @brief Prints imgStore header informations.
 *
 * @param header The header to be displayed.
 */
void print_header (const struct imgst_header * header);

/**
 * @brief Prints image metadata informations.
 *
 * @param metadata The metadata of one image.
 */
void print_metadata (const struct img_metadata * metadata);

/**
 * @brief Open imgStore file, read the header and all the metadata.
 *
 * @param imgst_filename Path to the imgStore file
 * @param open_mode Mode for fopen(), eg.: "rb", "rb+", etc.
 * @param imgst_file Structure for header, metadata and file pointer.
 */
int do_open (const char * imgst_filename, const char * open_mode, struct imgst_file* imgst_file);

/**
 * @brief Do some clean-up for imgStore file handling.
 *
 * @param imgst_file Structure for header, metadata and file pointer to be freed/closed.
 */
void do_close (struct imgst_file* imgst_file);

/**
 * @brief List of possible output modes for do_list
 *
 * @param imgst_file Structure for header, metadata and file pointer to be freed/closed.
 */
typedef enum { STDOUT, JSON } do_list_mode;

/**
 * @brief Displays (on stdout or JSON) imgStore metadata.
 *
 * @param imgst_file In memory structure with header and metadata.
 * @param mode Output mode.
*/
char* do_list (const struct imgst_file * imgst_file, do_list_mode mode);

/**
 * @brief Creates the imgStore called imgst_filename. Writes the header and the
 *        preallocated empty metadata array to imgStore file.
 *
 * @param imgst_filename Path to the imgStore file
 * @param imgst_file In memory structure with header and metadata.
 */
int do_create (const char* imgst_filename, struct imgst_file* imgst_file);

/**
 * @brief Deletes an image from a imgStore imgStore.
 *
 * Effectively, it only invalidates the is_valid field and updates the
 * metadata.  The raw data content is not erased, it stays where it
 * was (and  new content is always appended to the end; no garbage
 * collection).
 *
 * @param img_id The ID of the image to be deleted.
 * @param imgst_file The main in-memory data structure
 * @return Some error code. 0 if no error.
 */
int do_delete(const char* img_id, struct imgst_file* imgst_file);

/**
 * @brief Transforms resolution string to its int value.
 *
 * @param resolution The resolution string. Shall be "original",
 *        "orig", "thumbnail", "thumb" or "small".
 * @return The corresponding value or -1 if error.
 */
int resolution_atoi(const char* resolution);

/**
 * @brief Reads the content of an image from a imgStore.
 *
 * @param img_id The ID of the image to be read.
 * @param resolution The desired resolution for the image read.
 * @param image_buffer Location of the location of the image content
 * @param image_size Location of the image size variable
 * @param imgst_file The main in-memory data structure
 * @return Some error code. 0 if no error.
 */
int do_read(const char* img_id, int resolution, char** image_buffer, uint32_t* image_size, struct imgst_file* imgst_file);

/**
 * @brief Insert image in the imgStore file
 *
 * @param buffer Pointer to the raw image content
 * @param size Image size
 * @param img_id Image ID
 * @param imgst_file The main in-memory data structure
 * @return Some error code. 0 if no error.
 */
int do_insert(const char* buffer, size_t size, const char* img_id, struct imgst_file* imgst_file);

/**
 * @brief Removes the deleted images by moving the existing ones
 *
 * @param imgst_path The path to the imgStore file
 * @param imgst_tmp_bkp_path The path to the a (to be created) temporary imgStore backup file
 * @return Some error code. 0 if no error.
 */
int do_gbcollect (const char *imgst_path, const char *imgst_tmp_bkp_path);


// ======================================== Additional methods ========================================
/**
 * @brief (Additional) Updates the metadata of the image at position 'index' in memory.
 *
 * @param imgst_file The main in-memory data structure.
 * @param index The position of the image in memory.
 */
int update_metadata (struct imgst_file * imgst_file, const size_t index);

/**
 * @brief (Additional) Updates the header of the image store file on disk.
 *
 * @param imgst_file The main in-memory data structure.
 */
int update_header (struct imgst_file * imgst_file);

/**
 * @brief (Additional) Compares the two given SHA values.
 *
 * @param sha1 SHA value 1
 * @param sha2 SHA value 2
 */
int compare_sha (const unsigned char* sha1, const unsigned char* sha2);

/**
 * @brief (Additional) Writes the image (buffer content) at the end of the imgStore file.
 *
 * @param index The index of the image in the metadata.
 * @param res Image resolution.
 * @param buffer Pointer to the raw image content.
 * @param size Image size.
 * @param imgst_file The main in-memory data structure.
 * @return Some error code. 0 if no error.
 */
int write_image_end_of_imgst (size_t index, const int res, const char* buffer, size_t size, struct imgst_file* imgst_file);

/**
 * @brief (Additional) Loads in the given buffer the image at position 'index' in the imgStore file.
 *
 * @param index The index of the image in the metadata.
 * @param resolution The desired resolution for the image read.
 * @param image_buffer Buffer to contain the image.
 * @param image_size Image size.
 * @param imgst_file The main in-memory data structure.
 * @return Some error code. 0 if no error.
 */
int load_image_from_imgst (size_t index, const int resolution, char* image_buffer, uint32_t image_size, struct imgst_file* imgst_file);

#ifdef __cplusplus
}
#endif
#endif
