/**
 * @file imgStore_server.c
 * @brief Web server implementation.
 */

#include "imgStore.h"
#include "mongoose.h"
#include "error.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vips/vips.h>

// ======================================================================
static const char* s_listening_address = "http://localhost:8000";
static struct imgst_file imgst_file;

// ======================================================================
#define MAX_IMG_RES 10  // (Additional) max. size of an image resolution variable
#define MAX_OFFSET 40   // (Additional) max. size of an image size variable

// ======================================================================
/**
 * @brief Returns a HTTP 500 error code along with the imgStore error.
 *
 * @param nc The connection.
 * @param error The imgStore error.
 */
void mg_error_msg(struct mg_connection* nc, int error)
{
    mg_http_reply(nc, 500, "", "Error: %s\n", ERR_MESSAGES[error]);
}

// ======================================================================
/**
 * @brief (Additional) Checks if an error occured when decoding an HTTP variable.
 *
 * @param nc The connection.
 * @param len The length of the decoded HTTP variable.
 */
int arg_tests(struct mg_connection* nc, int len) {
    if (len <= 0) {
        mg_error_msg(nc, ERR_INVALID_ARGUMENT);
        return 1;
    }
    return 0;
}

// ======================================================================
/**
 * @brief (Additional) Checks if an error occured when decoding an 'img_id' variable.
 *
 * @param nc The connection.
 * @param img_id_len The length of the decoded 'img_id' variable.
 */
int arg_tests_img_id(struct mg_connection* nc, int img_id_len) {
    if (arg_tests(nc, img_id_len)) return 1;
    if (img_id_len > MAX_IMG_ID) {
        mg_error_msg(nc, ERR_INVALID_IMGID);
        return 1;           
    }
    return 0;
}

// ======================================================================
/**
 * @brief (Additional) Refreshes the HTML page 'index.html' if the given command yields no error.
 *
 * @param nc The connection.
 * @param error_cmd The error of the command.
 */
static void refresh_page(struct mg_connection* nc, int error_cmd)
{
    if (error_cmd == ERR_NONE) {
        // HTTP response that will reload the page 'index.html'
        mg_printf(nc, "HTTP/1.1 302 Found\r\nLocation: %s/index.html\r\n\r\n", s_listening_address);
    } else {
        mg_error_msg(nc, error_cmd);
    }
}

// ======================================================================
/**
 * @brief Handles the 'list' call.
 *
 * @param nc The connection.
 */
static void handle_list_call(struct mg_connection* nc)
{
    char* json = do_list(&imgst_file, JSON);
    if (json == NULL) return;

    mg_printf(nc, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", strlen(json), json);

    FREE_POINTER(json);
}

// ======================================================================
/**
 * @brief Handles the 'read' call, i.e. downloads an image.
 *
 * @param nc The connection.
 * @param hm HTTP GET message. 
 *           Example: http://localhost:8000/imgStore/read?res=orig&img_id=pic1
 */
static void handle_read_call(struct mg_connection* nc, struct mg_http_message* hm)
{
    // Gets the parameter 'res' (image resolution)
    char res[MAX_IMG_RES+1] = "";
    int len = mg_http_get_var(&(hm->query), "res", res, MAX_IMG_RES+1); // length of the decoded variable
    if (arg_tests(nc, len)) return;

    int resolution = resolution_atoi(res); // The corresponding resolution code
    if (resolution == -1) {
        mg_error_msg(nc, ERR_RESOLUTIONS);
        return;
    }

    // Gets the parameter 'imd_id' (image ID)
    char img_id[2*MAX_IMG_ID] = "";
    len = mg_http_get_var(&(hm->query), "img_id", img_id, 2*MAX_IMG_ID);
    if (arg_tests_img_id(nc, len)) return;


    char* image_buffer = NULL; // Location of the image content
    uint32_t image_size = 0; // Image size

    // Reads the image at the given resolution in the image buffer
    int error_read = do_read(img_id, resolution, &image_buffer, &image_size, &imgst_file);
    if (error_read == ERR_NONE) {
        mg_printf(nc, "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n", image_size);
        mg_send(nc, image_buffer, image_size); // Sends the content of the image
    } else {
        mg_error_msg(nc, error_read);
    }

    FREE_POINTER(image_buffer);
}

// ======================================================================
/**
 * @brief Handles the 'delete' call.
 *
 * @param nc The connection.
 * @param hm HTTP GET message. 
 *           Example: http://localhost:8000/imgStore/delete?img_id=pic1
 */
static void handle_delete_call(struct mg_connection* nc, struct mg_http_message* hm)
{
    // Gets the parameter 'img_id' (image ID)
    char img_id[2*MAX_IMG_ID] = "";
    int len = mg_http_get_var(&(hm->query), "img_id", img_id, 2*MAX_IMG_ID);
    if (arg_tests_img_id(nc, len)) return;

    // Deletes the image from the imgStore and refreshes the page
    int error_delete = do_delete(img_id, &imgst_file);
    refresh_page(nc, error_delete);
}

// ======================================================================
/**
 * @brief Handles the 'insert' call, i.e. uploads an image.
 *
 * @param nc The connection.
 * @param hm HTTP POST message, containing the content of the image to insert.
 *           Example: http://localhost:8000/imgStore/insert?name=foret.jpg&offset=1234
 */
static void handle_insert_call(struct mg_connection* nc, struct mg_http_message* hm)
{
    /* Collects and stores the chunks of the image file in the '/tmp' directory */
    if (hm->body.len != 0) {
        mg_http_upload(nc, hm, "/tmp");
        return;
    }

    /* Decodes the additional arguments when the last chunk has been received */
    // Gets the parameter 'name' (image ID)
    char name[2*MAX_IMG_ID] = "";
    int len = mg_http_get_var(&(hm->query), "name", name, 2*MAX_IMG_ID);
    if (arg_tests_img_id(nc, len)) return;

    // Gets the parameter 'offset' (image size)
    char offset[MAX_OFFSET+1] = "";
    len = mg_http_get_var(&(hm->query), "offset", offset, MAX_OFFSET+1);
    if (arg_tests(nc, len)) return;


    /* Reads the temporary storage file in 'tmp' */
    // Creates the filename '/tmp/name'
    char* tmp_name = calloc(strlen("/tmp/") + strlen(name), 1);
    if (tmp_name == NULL) {
        mg_error_msg(nc, ERR_OUT_OF_MEMORY);
        return;
    }
    strcpy(tmp_name, "/tmp/");
    strcat(tmp_name, name);

    // Temporary storage file
    FILE* tmp_file = fopen(tmp_name, "rb");
    FREE_POINTER(tmp_name);
    if (tmp_file == NULL) {
        mg_error_msg(nc, ERR_IO);
        return;
    }


    /* Inserts the image file in the buffer */
    size_t size = atouint32(offset); // Image size
    char* buffer = malloc(size+1); // Pointer to the raw image content
    if (buffer == NULL) {
        fclose(tmp_file);
        mg_error_msg(nc, ERR_OUT_OF_MEMORY);
        return;
    }

    size_t nb_ok = fread(buffer, 1, size, tmp_file);
    fclose(tmp_file);
    if (nb_ok != size) {
        FREE_POINTER(buffer);
        mg_error_msg(nc, ERR_IO);
        return;
    }

    /* Inserts the image in the imgStore and refreshes the page */
    int error_insert = do_insert(buffer, size, name, &imgst_file);
    refresh_page(nc, error_insert);

    FREE_POINTER(buffer);
}

// ======================================================================
/**
 * @brief Handles URIs.
 *
 * @param nc The connection that received an event.
 * @param hm HTTP message.
 * @param ev_data Event-specific data.
 */
static void imgst_event_handler(struct mg_connection* nc, struct mg_http_message* hm, void* ev_data)
{   
    if (mg_http_match_uri(hm, "/imgStore/list") && !strncmp("GET", hm->method.ptr, 3)) {
        handle_list_call(nc);
    } else if (mg_http_match_uri(hm, "/imgStore/read") && !strncmp("GET", hm->method.ptr, 3)) {
        handle_read_call(nc, hm);
    } else if (mg_http_match_uri(hm, "/imgStore/delete") && !strncmp("GET", hm->method.ptr, 3)) {
        handle_delete_call(nc, hm);
    } else if (mg_http_match_uri(hm, "/imgStore/insert") && !strncmp("POST", hm->method.ptr, 4)) {
        handle_insert_call(nc, hm);
    } else {
        struct mg_http_serve_opts opts = { .root_dir = "." };
        mg_http_serve_dir(nc, hm, &opts);
    }
}

// ======================================================================
/**
 * @brief Handles server events (eg HTTP requests).

 * @param nc The connection that received an event.
 * @param ev Type of the event.
 * @param ev_data Event-specific data.
 * @param fn_data Holds application-specific data.
 */
static void event_handler(struct mg_connection* nc, int ev, void* ev_data, void* fn_data)
{
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = (struct mg_http_message*) ev_data;
        imgst_event_handler(nc, hm, ev_data);
        nc->is_draining = 1;
    }
}

// ======================================================================
int main (int argc, char* argv[])
{
    int ret = 0;

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {

        if (!VIPS_INIT(argv[0])) {
            M_EXIT_IF_ERR(do_open(argv[1], "rb+", &imgst_file));

            // Start mongoose server
            struct mg_mgr mgr; // Event manager, that holds all active connections
            mg_mgr_init(&mgr);
            if (mg_http_listen(&mgr, s_listening_address, event_handler, NULL) == NULL) {
                fprintf(stderr, "http server could not be initialized\n");
                return -1;
            }
            printf("Starting imgStore server on %s\n", s_listening_address);
            print_header(&(imgst_file.header));

            // Poll
            for (;;) mg_mgr_poll(&mgr, 1000);

            // Shutdown mongoose server
            mg_mgr_free(&mgr);

            vips_shutdown();

            do_close(&imgst_file);

        } else {
            vips_error_exit("unable to start VIPS");
            ret = ERR_IMGLIB;
        }
    }

    if (ret) {
        fprintf(stderr, "%s\n", ERR_MESSAGES[ret]);
    }

    return ret;
}