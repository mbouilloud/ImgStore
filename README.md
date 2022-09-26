# ImgStore, an image-oriented file system
Image database manager, based on Facebook's "Haystack". EPFL CS-212 Project, Spring 2021. 

## Required libraries:
- `libssl-dev`: cryptography library
- `libvips-dev`: image management library
- `libjson-c-dev`: library for JSON communication with the webserver

## Using ImgStore:
- On the command line (For help : `./imgStoreMgr help`) : 

```sh
# Create an ImgStore file
./imgStoreMgr create imgst_file 

# Insert a picture (sample images available in `/tests/data`)
./imgStoreMgr insert imgst_file pic1 coquelicots.jpg

# Read it in a different resolution, for instance thumbnail
./imgStoreMgr read imgst_file pic1 thumbnail

# List the ImgStore's content
/imgStoreMgr list imgst_file

# Delete a picture
./imgStoreMgr delete imgst_file pic1

```

- On the webserver :
```sh
  export LD_LIBRARY_PATH="${PWD}/libmongoose"
  cd tests/data                    # go where index.html is
  cp test02.imgst_dynamic test.db  # making a safe working copy
  ../../imgStore_server test.db
```
