// RLE Encoder.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <vector>
#include "include/bitmap.h"

using namespace std;

int main()
{
    CBitmap bitmap("test.bmp");

    int metatile_size = 16;
    set<string> metatiles;
    RGBA* bits = (RGBA*) bitmap.GetBits();

    // Figure out what our metatiles are for final output
    for (int i = 0; i < bitmap.GetHeight(); i += metatile_size)
    {
        for (int j = i; j < bitmap.GetWidth() + i; j += metatile_size)
        {
            string tile_string = make_tile_string(bits + j, bitmap.GetWidth(), metatile_size);
            metatiles.insert(tile_string);
        }
    }

    map<string, char> metatile_codes;
    char code = 0;
    for (auto metatile : metatiles)
    {
        metatile_codes[metatile] = code;
        code++;
    }
}

/*
* Coding is Konami RLE
* 
* $00-80 - The next byte is repeated n times
* $81-FE - The next 128-n bytes are literals
* $FF - End of stream
* 
* Need at least three repeated to be worth using the repeated form
*/

string rle_row(RGBA* bit_start, int width, int tile_size, map<string, char>& metatile_codes)
{
    // Python example: https://github.com/sobodash/graveyardduck/blob/master/graveduck.py
    string curr_tile_string = "";
    int curr_length = 0;
    stringstream ss;
    for (int i = 0; i < width; i += tile_size)
    {
        string tile_string = make_tile_string(bit_start + i, width, tile_size);
        if (curr_length == 0)
        {
            curr_tile_string = tile_string;
            curr_length++;
            continue;
        }

        if (tile_string != curr_tile_string)
        {
            // TODO: Need to split between length > 2 and not
            // If length > 2 then we do the simple repeated formulation
            // If length 1-2 then we add it to what we're building
            
        }
    }


    return ss.str();
}

/*
* Converts a vector of chars to a string representation ready for concatination.
* e.g. 0x01, 0xA3, 0xFF -> "$01", "$A3", "$FF"
*/
void chars_to_hex(vector<char>& input, vector<string>& output)
{
    for (auto& el : input)
    {
        stringstream ss;
        ss << "$";
        ss << setfill('0') << setw(2) << hex << (0xff & (unsigned int)el);
        output.push_back(ss.str());
    }
}

string make_tile_string(RGBA* bit_start, int width, int tile_size)
{
    stringstream ss;
    for (int i = 0; i < tile_size; i += width)
    {
        for (int j = i; j < i + tile_size; j++)
        {
            RGBA* pixel = bit_start + j;
            ss << pixel->Red << pixel->Green << pixel->Blue;
        }
    }

    return ss.str();
}