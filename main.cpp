#include <filesystem>
#include <iostream>
#include <algorithm>

#include <math.h>
#include <png.h>
#include <jpeglib.h>

namespace fs = std::filesystem;
 
int main()
{
    fs::path current_dir("/home/cassiano.old/Pictures/");

    for(auto &file : fs::recursive_directory_iterator(current_dir))
    {
        std::string s(file.path().extension());
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        if(s == ".jpg")
            std::cout << file.path().string() << std::endl;
    }
}