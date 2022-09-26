/**
 * @file imgStoreMgr.c
 * @brief imgStore Manager: command line interpretor for imgStore core commands.
 *
 * Image Database Management Tool
 *
 * @author Mia Primorac
 */

#include "util.h" // for _unused
#include "imgStore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vips/vips.h>

typedef int (*command)(int, char*[]);

typedef struct {
    const char* name;
    const command command;
} command_mapping;

int write_disk_image(const char* img_id, const char* resolution_suffix, const char* image_buffer, size_t image_size);
int read_disk_image(char** buffer, size_t* size, const char* filename);
void create_name(const char* img_id, const char* resolution_suffix, char* filename, size_t name_length);

/********************************************************************//**
 * Opens imgStore file and calls do_list command.
 ********************************************************************** */
int
do_list_cmd (int args, char* argv[])
{
    if (args == 1) return ERR_NOT_ENOUGH_ARGUMENTS;
    const char* filename = argv[1];

    struct imgst_file myfile;

    M_EXIT_IF_ERR(do_open(filename, "rb", &myfile));
    do_list(&myfile, STDOUT);

    do_close(&myfile);

    return ERR_NONE;
}

/********************************************************************//**
 * Prepares and calls do_create command.
********************************************************************** */
int
do_create_cmd (int args, char* argv[])
{
    if (args == 1) return ERR_NOT_ENOUGH_ARGUMENTS; // < 2
    const char* filename = argv[1];

    uint32_t max_files   =  10;
    uint16_t thumb_res_x =  64;
    uint16_t thumb_res_y =  64;
    uint16_t small_res_x = 256;
    uint16_t small_res_y = 256;

    // Parsing of command line arguments
    for (size_t i = 2; i < args; ++i) {
        if (!strcmp(argv[i], "-max_files")) {
            if (args - i > 1) {
                max_files = atouint32(argv[i+1]);
                ++i;
                if (max_files == 0 || max_files > MAX_MAX_FILES) return ERR_MAX_FILES;
            } else {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
        } else if (!strcmp(argv[i], "-thumb_res")) {
            if (args - i > 2) {
                thumb_res_x = atouint16(argv[i+1]);
                thumb_res_y = atouint16(argv[i+2]);
                i += 2;
                if (thumb_res_x == 0 || thumb_res_x > 128 || thumb_res_y == 0 || thumb_res_y > 128) return ERR_RESOLUTIONS;
            } else {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
        } else if (!strcmp(argv[i], "-small_res")) {
            if (args - i > 2) {
                small_res_x = atouint16(argv[i+1]);
                small_res_y = atouint16(argv[i+2]);
                i += 2;
                if (small_res_x == 0 || small_res_x > 512 || small_res_y == 0 || small_res_y > 512) return ERR_RESOLUTIONS;
            } else {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
        } else {
            return ERR_INVALID_ARGUMENT;
        }
    }

    puts("Create");

    struct imgst_header header = {
        .max_files = max_files,
        .res_resized = {
            thumb_res_x,
            thumb_res_y,
            small_res_x,
            small_res_y
        }
    };

    struct imgst_file imgst_file = { .header = header };

    // Creates the new image database in a binary file on disk
    M_EXIT_IF_ERR(do_create(filename, &imgst_file));
    print_header(&imgst_file.header);

    do_close(&imgst_file);

    return ERR_NONE;
}

/********************************************************************//**
 * Displays some explanations.
 ********************************************************************** */
int
help (int args, char* argv[])
{
    printf(
    "imgStoreMgr [COMMAND] [ARGUMENTS]\n"
    "  help: displays this help.\n"
    "  list <imgstore_filename>: list imgStore content.\n"
    "  create <imgstore_filename> [options]: create a new imgStore.\n"
    "      options are:\n"
    "          -max_files <MAX_FILES>: maximum number of files.\n"
    "                                  default value is 10\n"
    "                                  maximum value is 100000\n"
    "          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n"
    "                                  default value is 64x64\n"
    "                                  maximum value is 128x128\n"
    "          -small_res <X_RES> <Y_RES>: resolution for small images.\n"
    "                                  default value is 256x256\n"
    "                                  maximum value is 512x512\n"
    "  read <imgstore_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n"
    "      read an image from the imgStore and save it to a file.\n"
    "      default resolution is \"original\".\n"
    "  insert <imgstore_filename> <imgID> <filename>: insert a new image in the imgStore.\n"
    "  delete <imgstore_filename> <imgID>: delete image imgID from imgStore.\n"
    "  gc <imgstore_filename> <tmp imgstore_filename>: performs garbage collecting on imgStore. Requires a temporary filename for copying the imgStore.\n");
    return 0;
}

/********************************************************************//**
 * Deletes an image from the imgStore.
 */
int
do_delete_cmd (int args, char* argv[])
{
    if (args < 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char* filename = argv[1];
    const char* imgID = argv[2];
    if (strlen(imgID) <= 0 || strlen(imgID) > MAX_IMG_ID) return ERR_INVALID_IMGID;

    struct imgst_file myfile;

    // Reads the file
    M_EXIT_IF_ERR(do_open(filename, "rb+", &myfile));

    // Deletes the image which has ID 'imgID';
    int error_delete = do_delete(imgID, &myfile);

    // Closes the file
    do_close(&myfile);

    return error_delete;
}

/********************************************************************//**
 * Reads an image from the imgStore.
********************************************************************** */
int
do_read_cmd (int args, char* argv[])
{
    if (args < 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    // Command line arguments
    int resolution;
    const char* resolution_suffix; // The desired resolution for the image read.
    if (args == 4) {
        resolution = resolution_atoi(argv[3]);
        switch (resolution) {
        case RES_ORIG: resolution_suffix = "orig"; break;
        case RES_THUMB: resolution_suffix = "thumb"; break;
        case RES_SMALL: resolution_suffix = argv[3]; break;
        default: return ERR_RESOLUTIONS;
        }
    } else {
        resolution = RES_ORIG;
        resolution_suffix = "orig";
    }
    const char* imgstore_filename = argv[1]; // name of the database file
    const char* img_id = argv[2];            // The ID of the image to be read
    if (strlen(img_id) == 0 || strlen(img_id) > MAX_IMG_ID) return ERR_INVALID_IMGID;


    // Reads the buffer content from the imgStore
    struct imgst_file myfile;
    M_EXIT_IF_ERR(do_open(imgstore_filename, "rb+", &myfile));

    char* image_buffer = NULL; // Location of the image content
    uint32_t image_size = 0; // Image size

    int error = do_read(img_id, resolution, &image_buffer, &image_size, &myfile);
    if (error == ERR_NONE) {
        error = write_disk_image(img_id, resolution_suffix, image_buffer, (size_t) image_size);
    }

    do_close(&myfile);
    FREE_POINTER(image_buffer);

    return error;
}

/********************************************************************//**
 * Inserts an image in the imgStore.
********************************************************************** */
int
do_insert_cmd (int args, char* argv[])
{
    if (args < 4) return ERR_NOT_ENOUGH_ARGUMENTS;

    // Command line arguments
    const char* imgstore_filename = argv[1]; // name of the imgStore
    const char* img_id = argv[2]; // Image ID
    if (strlen(img_id) == 0 || strlen(img_id) > MAX_IMG_ID) return ERR_INVALID_IMGID;
    const char* filename = argv[3]; // name of the image file

    char* buffer = NULL;
    size_t size = 0;
    M_EXIT_IF_ERR(read_disk_image(&buffer, &size, filename));

    // Inserts the buffer content in the imgStore
    struct imgst_file myfile;
    M_EXIT_IF_ERR(do_open(imgstore_filename, "rb+", &myfile));

    int error_insert = do_insert(buffer, size, img_id, &myfile);

    do_close(&myfile);
    FREE_POINTER(buffer);

    return error_insert;
}

/********************************************************************//**
 * Garbage collection of the imgStore.
********************************************************************** */
int
do_gc_cmd (int args, char* argv[])
{
    if (args < 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    return do_gbcollect(argv[1], argv[2]);
}

/**
 * @brief Writes the image on the disk (i.e. creates a new JPEG file).
 *
 * @param img_id Image ID.
 * @param resolution_suffix ("thumb", "small", "orig").
 * @param image_buffer Buffer containing the image (bytes).
 * @param image_size Image size (in bytes).
 * @return Some error code. 0 if no error.
 */
int
write_disk_image(const char* img_id, const char* resolution_suffix, const char* image_buffer, size_t image_size)
{
    // Creates the image filename
    size_t name_length = strlen(img_id) + 1 + strlen(resolution_suffix) + strlen(".jpg");
    char* filename = calloc(name_length+1, 1);
    M_EXIT_IF_NULL(filename, name_length+1);
    create_name(img_id, resolution_suffix, filename, name_length);

    // Opens the corresponding image file
    FILE* image_file = fopen(filename, "wb");
    FREE_POINTER(filename);
    if (image_file == NULL) return ERR_IO;
    
    // Writes the content of the buffer to the image file
    int error = ERR_NONE;
    if (fwrite(image_buffer, 1, image_size, image_file) != image_size) {
        error = ERR_IO;
    }

    fclose(image_file);

    return error;
}

/**
 * @brief Reads the image from the disk (i.e. loads the JPEG file in a buffer).
 *
 * @param buffer Points to the buffer to contain the image.
 * @param size Points to the image size (in bytes).
 * @param filename Name of the image file.
 * @return Some error code. 0 if no error.
 */
int
read_disk_image(char** buffer, size_t* size, const char* filename)
{
    // Opens the image file
    FILE* image_file = fopen(filename, "rb");
    if (NULL == image_file) return ERR_IO;

    // Allocates a buffer that will contain the bytes of the image
    fseek(image_file, 0, SEEK_END);
    *size = ftell(image_file); // Image size
    *buffer = malloc(*size + 1); // Pointer to the raw image content
    if (*buffer == NULL) {
        fclose(image_file);
        return ERR_OUT_OF_MEMORY;
    }

    // Loads the image file in the allocated buffer
    rewind(image_file);
    size_t nb_read = fread(*buffer, 1, *size, image_file);
    fclose(image_file);
    if (nb_read != *size) {
        FREE_POINTER(*buffer);
        return ERR_IO;
    }

    return ERR_NONE;
}

/**
 * @brief (Additional) Creates the name of an image file.
 *
 * @param img_id The ID of the image in the imgStore.
 * @param resolution_suffix "_orig" or "_small" or "_thumb".
 * @param filename The name of the image file.
 * @param name_length The length of the created name.
 */
void create_name(const char* img_id, const char* resolution_suffix, char* filename, size_t name_length)
{
    strcat(filename, img_id);
    strcat(filename, "_");
    strcat(filename, resolution_suffix);
    strcat(filename, ".jpg");
    filename[name_length] = '\0';
}

/********************************************************************//**
 * MAIN
 */
int main (int argc, char* argv[])
{
    size_t nb_commands = 7;
    command_mapping commands[] = {
        {"help", help},
        {"list", do_list_cmd},
        {"create", do_create_cmd},
        {"read", do_read_cmd},
        {"insert", do_insert_cmd},
        {"delete", do_delete_cmd},
        {"gc", do_gc_cmd}
    };

    int ret = 0;

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {

        if (VIPS_INIT(argv[0])) {
            vips_error_exit("unable to start VIPS");
            return ERR_IMGLIB;
        }

        argc--; argv++; // skips command call name

        size_t found = 0; // whether the command has been found or not
        for (size_t i = 0; !found && i < nb_commands; ++i) {
            if (!strcmp(commands[i].name, argv[0])) {
                ret = commands[i].command(argc, argv);
                found = 1;
            }
        }
        if (!found) ret = ERR_INVALID_COMMAND;

        vips_shutdown();
    }

    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MESSAGES[ret]);
        help(argc, argv);
    }

    return ret;
}
