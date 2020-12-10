#include <algorithm>
#include <filesystem>
#include <iostream>

#include <memory.h>
#include <jpeglib.h>
#include <math.h>
#include <png.h>

namespace fs = std::filesystem;

struct image
{
    unsigned num_components;
    unsigned width;
    unsigned height;
    unsigned output_width;
    unsigned output_height;
    J_COLOR_SPACE colorspace;

    unsigned scalewidth;
    unsigned scaleheight;
    unsigned char* data;
    FILE* outfile;
};

int load_image_jpeg(struct image* img, const char* filename)
{
    struct jpeg_decompress_struct dinfo;
    struct jpeg_error_mgr jerr_mgr;

    unsigned char *pr;
    char has_decompress_started = 0;
    unsigned row_width;
    JSAMPARRAY samp;
    FILE *infile;

    dinfo.err = jpeg_std_error(&jerr_mgr);
    jerr_mgr.error_exit = [](j_common_ptr cinfo){throw cinfo->err;};

    if ((infile = fopen(filename, "rb")) == NULL)
    {
        fprintf(stderr, "can't fopen(%s): %s\n", filename, strerror(errno));
        return 0;
    }

    jpeg_create_decompress(&dinfo);
    jpeg_stdio_src(&dinfo, infile);
    jpeg_read_header(&dinfo, FALSE);

    img->width = dinfo.image_width;
    img->height = dinfo.image_height;
    img->num_components = dinfo.num_components;

    // if (!compute_scaledims(img, 1))
    // {
    //     jpeg_destroy_decompress(&dinfo);
    //     return 0;
    // }

    /*
     * Use libjpeg's handy feature to downscale the
     * original on the fly while reading it in.
     */
    if (img->width >= 8 * img->scalewidth)
        dinfo.scale_denom = 8;
    else if (img->width >= 4 * img->scalewidth)
        dinfo.scale_denom = 4;
    else if (img->width >= 2 * img->scalewidth)
        dinfo.scale_denom = 2;

    has_decompress_started = 1;
    jpeg_start_decompress(&dinfo);
    img->output_width = dinfo.output_width;
    img->output_height = dinfo.output_height;
    img->colorspace = dinfo.out_color_space;
    row_width = dinfo.output_width * img->num_components;

    img->data = (unsigned char *)malloc(row_width * dinfo.output_height * sizeof(unsigned char));

    samp = (*dinfo.mem->alloc_sarray)((j_common_ptr)&dinfo, JPOOL_IMAGE, row_width, 1);

    /* Read the image into memory. */
    pr = img->data;
    while (dinfo.output_scanline < dinfo.output_height)
    {
        jpeg_read_scanlines(&dinfo, samp, 1);
        memcpy(pr, *samp, row_width * sizeof(char));
        pr += row_width;
    }

    jpeg_finish_decompress(&dinfo);
    jpeg_destroy_decompress(&dinfo);

    /* if (setjmp(...)) above can't happen anymore. */
    return 1;
}

int main()
{
    fs::path current_dir("/home/cassiano.old/Pictures/");

    for (auto& file : fs::recursive_directory_iterator(current_dir))
    {
        std::string s(file.path().extension());
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        if (s == ".jpg")
        {
            struct image i;
            load_image_jpeg(&i, file.path().string().c_str());
            std::cout << file.path().string() << std::endl;
            break;
        }
    }
}