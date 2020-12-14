#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>

#include <jpeglib.h>
#include <math.h>
#include <memory.h>
#include <png.h>

namespace fs = std::filesystem;

int def_scaleheight = 480;

struct image
{
    unsigned num_components;
    unsigned width;
    unsigned height;
    unsigned output_width;
    unsigned output_height;
    J_COLOR_SPACE colorspace;

    std::string in_filename;
    std::string out_filename;

    unsigned scalewidth;
    unsigned scaleheight;
    unsigned char* data;
};

namespace Swag
{
    bool load_image_jpeg(image* img)
    {
        jpeg_decompress_struct dinfo;
        jpeg_error_mgr jerr_mgr;

        unsigned char* pr;
        unsigned row_width;
        JSAMPARRAY samp;
        FILE* infile;

        dinfo.err = jpeg_std_error(&jerr_mgr);
        jerr_mgr.error_exit = [](j_common_ptr cinfo) { throw cinfo->err; };

        if ((infile = fopen(img->in_filename.c_str(), "rb")) == NULL)
        {
            std::cout << "can't fopen " << img->in_filename << ": " << strerror(errno);
            return false;
        }

        jpeg_create_decompress(&dinfo);
        jpeg_stdio_src(&dinfo, infile);
        jpeg_read_header(&dinfo, FALSE);

        img->width = dinfo.image_width;
        img->height = dinfo.image_height;
        img->num_components = dinfo.num_components;

        double ratio = (double)img->width / (double)img->height;
        img->scaleheight = def_scaleheight;
        img->scalewidth = (int)((double)img->scaleheight * ratio + 0.5);

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

        jpeg_start_decompress(&dinfo);
        img->output_width = dinfo.output_width;
        img->output_height = dinfo.output_height;
        img->colorspace = dinfo.out_color_space;
        row_width = dinfo.output_width * img->num_components;

        img->data = (unsigned char*)malloc(row_width * dinfo.output_height * sizeof(unsigned char));

        samp = (*dinfo.mem->alloc_sarray)((j_common_ptr)&dinfo, JPOOL_IMAGE, row_width, 1);

        pr = img->data;
        while (dinfo.output_scanline < dinfo.output_height)
        {
            jpeg_read_scanlines(&dinfo, samp, 1);
            memcpy(pr, *samp, row_width * sizeof(char));
            pr += row_width;
        }

        jpeg_finish_decompress(&dinfo);
        jpeg_destroy_decompress(&dinfo);

        fclose(infile);

        return true;
    }

    static void resize_bilinear(unsigned num_components, unsigned output_width, unsigned output_height, unsigned out_width, unsigned out_height,
                                const unsigned char* p, unsigned char* o)
    {
        double factor, fraction_x, fraction_y, one_minus_x, one_minus_y;
        unsigned ceil_x, ceil_y, floor_x, floor_y, s_row_width;
        unsigned tcx, tcy, tfx, tfy, tx, ty, t_row_width, x, y;

        /* RGB images have 3 components, grayscale images have only one. */
        s_row_width = num_components * output_width;
        t_row_width = num_components * out_width;
        factor = (double)output_width / (double)out_width;
        for (y = 0; y < out_height; y++)
        {
            for (x = 0; x < out_width; x++)
            {
                floor_x = (unsigned)(x * factor);
                floor_y = (unsigned)(y * factor);
                ceil_x = (floor_x + 1 > out_width) ? floor_x : floor_x + 1;
                ceil_y = (floor_y + 1 > out_height) ? floor_y : floor_y + 1;
                fraction_x = (x * factor) - floor_x;
                fraction_y = (y * factor) - floor_y;
                one_minus_x = 1.0 - fraction_x;
                one_minus_y = 1.0 - fraction_y;

                tx = x * num_components;
                ty = y * t_row_width;
                tfx = floor_x * num_components;
                tfy = floor_y * s_row_width;
                tcx = ceil_x * num_components;
                tcy = ceil_y * s_row_width;

                o[tx + ty] = one_minus_y * (one_minus_x * p[tfx + tfy] + fraction_x * p[tcx + tfy]) +
                            fraction_y * (one_minus_x * p[tfx + tcy] + fraction_x * p[tcx + tcy]);

                if (num_components != 1)
                {
                    o[tx + ty + 1] = one_minus_y * (one_minus_x * p[tfx + tfy + 1] + fraction_x * p[tcx + tfy + 1]) +
                                    fraction_y * (one_minus_x * p[tfx + tcy + 1] + fraction_x * p[tcx + tcy + 1]);

                    o[tx + ty + 2] = one_minus_y * (one_minus_x * p[tfx + tfy + 2] + fraction_x * p[tcx + tfy + 2]) +
                                    fraction_y * (one_minus_x * p[tfx + tcy + 2] + fraction_x * p[tcx + tcy + 2]);
                }
            }
        }
    }

    bool create_thumbnail(image* img)
    {
        jpeg_compress_struct cinfo;
        jpeg_error_mgr jerr_mgr;
        unsigned int img_datasize;
        unsigned char* o;
        JSAMPROW row_pointer[1];
        FILE *outfile;

        /* Resize the image. */
        img_datasize = img->scalewidth * img->scaleheight * img->num_components;

        if (img->output_width == img->scalewidth && (img->output_height == img->scaleheight || img->output_height == img->scaleheight + 1))
            o = img->data;
        else
        {
            o = (unsigned char*)malloc(img_datasize * sizeof(unsigned char));

            resize_bilinear(img->num_components, img->output_width, img->output_height, img->scalewidth, img->scaleheight, img->data, o);

            free(img->data);
        }
        img->data = NULL;

        if ((outfile = fopen(img->out_filename.c_str(), "wb")) == NULL)
        {
            std::cout << "Could not open " << strerror(errno) << std::endl;
            return 0;
        }

        cinfo.err = jpeg_std_error(&jerr_mgr);
        jerr_mgr.error_exit = [](j_common_ptr cinfo) { throw cinfo->err; };

        jpeg_create_compress(&cinfo);
        jpeg_stdio_dest(&cinfo, outfile);

        cinfo.image_width = img->scalewidth;
        cinfo.image_height = img->scaleheight;
        cinfo.input_components = img->num_components;
        cinfo.in_color_space = img->colorspace;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, 50, FALSE);
        jpeg_start_compress(&cinfo, FALSE);

        while (cinfo.next_scanline < cinfo.image_height)
        {
            row_pointer[0] = &o[cinfo.input_components * cinfo.image_width * cinfo.next_scanline];
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);

        fflush(outfile);
        fclose(outfile);

        jpeg_destroy_compress(&cinfo);
        free(o);

        return true;
    }

} // namespace Swag

int main()
{
    std::string basepath("/home/cassiano.old/Pictures/");
    fs::path current_dir(basepath);

    

    for (auto& file : fs::recursive_directory_iterator(current_dir))
    {
        std::string s(file.path().extension());
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        if (s == ".jpg")
        {
            image i;

            i.in_filename = file.path().string();
            i.out_filename = "teste.jpg";

            Swag::load_image_jpeg(&i);
            Swag::create_thumbnail(&i);

            std::cout << file.path().parent_path() << std::endl;

            break;
        }
    }
}