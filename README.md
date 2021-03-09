# RLE Encoder

A utility that takes in a bitmap and a tile size and RLE Encodes it using the Konami RLE scheme from Contra, plus an output of what the tiles are and their indexes.  This works best if the bitmap only has 4 used colors (e.g. NES tiles before attributes are applied) or else you lose compaction as two tiles that would be the same before attributes are seen as different.
