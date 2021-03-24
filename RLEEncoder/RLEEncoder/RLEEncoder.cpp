// RLE Encoder.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <vector>
#include "include/bitmap.h"
#include "include/cxxopts/cxxopts.hpp"

using namespace std;

/*
* Converts a vector of chars to a string representation ready for concatination.
* e.g. 0x01, 0xA3, 0xFF -> "$01", "$A3", "$FF"
*/
void chars_to_hex(const vector<char>& input, vector<string>& output)
{
    for (auto& el : input)
    {
        stringstream ss;
        ss << "$";
        ss << setfill('0') << setw(2) << hex << (0xff & (unsigned int)el);
        output.push_back(ss.str());
    }
}

/*
* Stringify a tile for easy comparison
*/
string make_tile_string(const RGBA* bit_start, int width, int tile_size)
{
    stringstream ss;
    for (int i = 0; i > -tile_size * width; i -= width)
    {
        for (int j = i; j < i + tile_size; j++)
        {
            const RGBA* pixel = bit_start + j;
            // Pixel will be converted to three chars in a string, which easily can be turned back into a bitmap with no alpha channel
            ss << pixel->Red << pixel->Green << pixel->Blue;
        }
    }

    return ss.str();
}

/*
* Output a bitmap for a stringified tile
*/
void output_bitmap(const string& tile, char code, int tile_size, const string& output_base)
{
    // Need to reverse the order of rows back to what bitmap is expecting
    const char* chars = tile.c_str();
    int length = tile.size();
    char* bits = new char[length];
    int k = 0;
    for (int i = (tile_size - 1) * tile_size * 3; i >= 0; i -= tile_size * 3) // * 3 because each pixel is 3 chars
    {
        for (int j = i; j < i + (tile_size * 3); j++)
        {
            bits[k] = chars[j];
            k++;
        }
    }

    CBitmap bitmap;
    bitmap.SetBits(bits, tile_size, tile_size, 0x0000FF, 0x00FF00, 0xFF0000);
    stringstream ss;
    ss << output_base << "-tile" << (int)code << ".bmp";  // need to cast or else it will render the char
    bitmap.Save(ss.str().c_str(), 24);
    delete[] bits;
}

/*
* Coding is Konami RLE
* 
* $00-80 - The next byte is repeated n times
* $81-FE - The next 128-n bytes are literals
* $FF - End of stream
* 
* Need at least three repeated to be worth using the repeated form
* 
* Adapted from Python example: https://github.com/sobodash/graveyardduck/blob/master/graveduck.py
* 
* Defaults to coding a horizontal row that begins at bit_start, but can instead code a vertical column if height is set.
* 
* bit_start - pointer to our initial position in the RGBA array
* width - how wide a line is
* tile_size - how big a tile is
* metatile_codes - mapping of tile_string to encoded identifier
* height - if greater than zero then set vertical encoding mode and use this as the height of the image
*/
string rle_encode(const RGBA* bit_start, int width, int tile_size, const map<string, char>& metatile_codes, int height = 0)
{
    int bit_end = width;
    int bit_step = tile_size;
    int sign = 1;
    if (height > 0)
    {
        bit_end = width * height;
        bit_step = tile_size * width;
        sign = -1;
    }

    vector<char> final_values;
    vector<char> running_tiles;
    int i = 0;
    while (i < bit_end)
    {
        string tile_string = make_tile_string(bit_start + (sign * i), width, tile_size);
        int count = 0;
        int last = i;

        // iterate through until we either reach the end or find a new tile
        while (i < bit_end && make_tile_string(bit_start + (sign * i), width, tile_size) == tile_string)
        {
            count++;
            i += bit_step;
        }

        // only if we have at least three repeated tiles should we bother encoding as a run
        if (count > 2)
        {
            // if we had a mishmash before encountering this run make sure we put that into the final first
            if (!running_tiles.empty())
            {
                final_values.push_back((char)0x80 + running_tiles.size());
                final_values.insert(final_values.end(), running_tiles.begin(), running_tiles.end());
            }

            // a run can only be so long
            if (count > 0x7F)
            {
                while (count > 0x7F)
                {
                    final_values.push_back(0x7F);
                    final_values.push_back(metatile_codes.at(tile_string));
                    count -= 0x7F;
                }
            }

            // encode the run and reset our state
            final_values.push_back((char)count);
            final_values.push_back(metatile_codes.at(tile_string));
            running_tiles.clear();
        }
        else // need to collect the random tiles that will be literals
        {
            // intialize the literals
            if (running_tiles.empty())
            {
                // will only ever be 1 or 2
                for (int j = last; j < i; j += bit_step)
                {
                    string temp_tile_string = make_tile_string(bit_start + (sign * j), width, tile_size);
                    running_tiles.push_back(metatile_codes.at(temp_tile_string));
                }
            }
            else // already had a collection of literals
            {
                // if our size is too big then we need to flush and start a new segment
                if (running_tiles.size() > 0xFC - 0x80)
                {
                    final_values.push_back((char)0x80 + running_tiles.size());
                    final_values.insert(final_values.end(), running_tiles.begin(), running_tiles.end());
                    running_tiles.clear();
                }
                else
                {
                    // will only ever be 1 or 2
                    for (int j = last; j < i; j += bit_step)
                    {
                        string temp_tile_string = make_tile_string(bit_start + (sign * j), width, tile_size);
                        running_tiles.push_back(metatile_codes.at(temp_tile_string));
                    }
                }
            }
        }
    }

    // get any leftover unencoded stuff
    if (!running_tiles.empty())
    {
        final_values.push_back((char)0x80 + running_tiles.size());
        final_values.insert(final_values.end(), running_tiles.begin(), running_tiles.end());
    }

    // terminate the string
    final_values.push_back((char)0xFF);

    vector<string> converted_values;
    chars_to_hex(final_values, converted_values);

    stringstream ss;
    for_each(converted_values.begin(), converted_values.end(), [&ss](string& s) { ss << s << ", "; });
    string retval = ss.str();
    // erase trailing ", "
    retval.pop_back();
    retval.pop_back();

    return retval;
}

void print_usage(const cxxopts::Options& options)
{
    cout << options.help() << endl;
}

bool validate_args(const cxxopts::ParseResult& result, string& map_name, string& output_base, int& tile_size)
{
    map_name = result["map"].as<string>();
    if (map_name.empty())
    {
        cout << "No map provided" << endl;
        return false;
    }

    output_base = result["output"].as<string>();
    if (output_base.empty())
    {
        cout << "Empty output provided" << endl;
    }

    tile_size = result["tileSize"].as<int>();
    if (tile_size < 8)
    {
        cout << "Tile size must be at least 8 pixels" << endl;
        return false;
    }

    return true;
}

int main(int argc, char** argv)
{
    cxxopts::Options options("RLE Encoder", "Utility to RLE encode a bitmap using the Konami algorithm");
    options.add_options()
        ("m,map", "Map to parse and encode", cxxopts::value<string>()->default_value(""))
        ("o,output", "Base name for output files (default: out)", cxxopts::value<string>()->default_value("out"))
        ("t,tileSize", "Size of tiles to RLE encode (default: 16", cxxopts::value<int>()->default_value("16"))
        ("h,help", "Print usage")
        ;

    string map_name;
    string output_base;
    int metatile_size;
    try
    {
        auto result = options.parse(argc, argv);
        if (result.count("help"))
        {
            print_usage(options);
            exit(0);
        }

        if (!validate_args(result, map_name, output_base, metatile_size))
        {
            print_usage(options);
            exit(1);
        }
    }
    catch (exception e)
    {
        print_usage(options);
        exit(0);
    }

    CBitmap bitmap;
    if (!bitmap.Load((map_name.c_str())))
    {
        cout << "Bitmap not found" << endl;
        exit(1);
    }

    if (bitmap.GetHeight() % metatile_size || bitmap.GetWidth() % metatile_size)
    {
        cout << "Bitmap dimensions must be evenly divisible by the tile size" << endl;
        exit(1);
    }

    set<string> metatiles;
    RGBA* bits = (RGBA*)bitmap.GetBits();

    // Bitmaps index from the lower left, but we want to output index from the upper left
    int upper_left = (bitmap.GetHeight() - 1) * bitmap.GetWidth();

    // Figure out what our metatiles are for final output
    // TODO: give caller the option to pass in a file that maps metatile bitmaps to their code and use that to
    // generate the metatile_codes map; would let us skip this for loop and the next one and give us control
    // over how things are encoded; especially important for attribute tables but also nice if you want your
    // metatiles to have some order to them, rather than the arbitrary order we get by generating it from the map.
    // But still leave in this method because it could be useful (hell, even if it's to do a first pass and create the
    // metatiles before then giving them your own codes and re-encoding the map)
    for (int i = upper_left; i >= 0; i -= metatile_size * bitmap.GetWidth())
    {
        for (int j = i; j < bitmap.GetWidth() + i; j += metatile_size)
        {
            string tile_string = make_tile_string(bits + j, bitmap.GetWidth(), metatile_size);
            metatiles.insert(tile_string);
        }
    }

    if (metatiles.size() > 256)
    {
        cout << "Too many metatiles generated at provided tile size: " << metatiles.size() << endl;
        exit(1);
    }

    map<string, char> metatile_codes;
    char code = 0;
    for (auto metatile : metatiles)
    {
        metatile_codes[metatile] = code;
        code++;
    }

    vector<string> horizontal_codings;
    // Encode horizontal strips
    for (int i = upper_left; i >= 0; i -= metatile_size * bitmap.GetWidth())
    {
        horizontal_codings.push_back(rle_encode(bits + i, bitmap.GetWidth(), metatile_size, metatile_codes));
    }

    vector<string> vertical_codings;
    // Encode vertical strips
    for (int i = upper_left; i < bitmap.GetWidth() + upper_left; i += metatile_size)
    {
        vertical_codings.push_back(rle_encode(bits + i, bitmap.GetWidth(), metatile_size, metatile_codes, bitmap.GetHeight()));
    }

    ofstream output;
    output.open(output_base + "-horizontal.txt");
    for (auto& coding : horizontal_codings)
    {
        output << coding << endl;
    }

    output.close();

    output.open(output_base + "-vertical.txt");
    for (auto& coding : vertical_codings)
    {
        output << coding << endl;
    }

    output.close();

    for (auto& entry : metatile_codes)
    {
        output_bitmap(entry.first, entry.second, metatile_size, output_base);
    }
}