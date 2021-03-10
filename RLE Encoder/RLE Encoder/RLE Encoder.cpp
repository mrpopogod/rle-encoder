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
* 
* Adapted from Python example: https://github.com/sobodash/graveyardduck/blob/master/graveduck.py
* 
* bit_start - pointer to our initial position in the RGBA array
* width - the width of a row
* tile_size - how big a tile is
* metatile_codes - mapping of tile_string to encoded identifier
* height - the height if we're encoding columns rather than rows
*/
string rle_encode(RGBA* bit_start, int bit_end, int bit_step, int width, int tile_size, map<string, char>& metatile_codes, int height = 0)
{
    int bit_end = width;
    int bit_step = tile_size;
    if (height)
    {
        bit_end = height * width;
        bit_step = tile_size * width;
    }

    vector<char> final_values;
    vector<char> running_tiles;
    int i = 0;
    while (i < bit_end)
    {
        string tile_string = make_tile_string(bit_start + i, width, tile_size);
        int count = 0;
        int last = i;

        // iterate through until we either reach the end or find a new tile
        while (i < bit_end && make_tile_string(bit_start + i, width, tile_size) == tile_string)
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
                    final_values.push_back(metatile_codes[tile_string]);
                    count -= 0x7F;
                }
            }

            // encode the run and reset our state
            final_values.push_back((char)count);
            final_values.push_back(metatile_codes[tile_string]);
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
                    string temp_tile_string = make_tile_string(bit_start + j, width, tile_size);
                    running_tiles.push_back(metatile_codes[temp_tile_string]);
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
                        string temp_tile_string = make_tile_string(bit_start + j, width, tile_size);
                        running_tiles.push_back(metatile_codes[temp_tile_string]);
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