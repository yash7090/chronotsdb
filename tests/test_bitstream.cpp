#include "../src/bitstream.h"
#include <cassert>
#include <iostream>
#include <bitset>
#include <random>
#include <vector>
#include <ctime>

static uint64_t rmask(int n)
{
    return n == 64 ? ~0ULL : ((1ULL << n) - 1);
}

int main()
{

    // easy case, everything fits in word 0
    {
        BitWriter w;
        w.write_bits(1, 1);
        w.write_bits(5, 3);
        BitReader r(w.words());
        assert(r.read_bits(1) == 1);
        assert(r.read_bits(3) == 5);
    }

    // 60 bits, then 8 that cross the word boundary, then 3 more (straddle)
    {
        BitWriter w;
        w.write_bits(0xFFFFFFFFFFFFFFFULL, 60);
        w.write_bits(0xAB, 8);
        w.write_bits(0x5, 3);
        BitReader r(w.words());
        assert(r.read_bits(60) == 0xFFFFFFFFFFFFFFFULL);
        assert(r.read_bits(8) == 0xAB);
        assert(r.read_bits(3) == 0x5);
    }

    // exact fill of a word, then continue
    {
        BitWriter w;
        w.write_bits(0x123, 32);
        w.write_bits(0x456, 32);
        w.write_bits(0x7, 3);
        BitReader r(w.words());
        assert(r.read_bits(32) == 0x123);
        assert(r.read_bits(32) == 0x456);
        assert(r.read_bits(3) == 0x7);
    }

    // n==64 into an empty word
    {
        BitWriter w;
        w.write_bits(0x5A5A5A5A5A5A5A5AULL, 64);
        w.write_bits(0x3, 2);
        BitReader r(w.words());
        assert(r.read_bits(64) == 0x5A5A5A5A5A5A5A5AULL);
        assert(r.read_bits(2) == 0x3);
    }

    // n==64 starting mid-word (always straddles)
    {
        BitWriter w;
        w.write_bits(0x5, 3);
        w.write_bits(0x5A5A5A5A5A5A5A5AULL, 64);
        w.write_bits(0x1, 1);
        BitReader r(w.words());
        assert(r.read_bits(3) == 0x5);
        assert(r.read_bits(64) == 0x5A5A5A5A5A5A5A5AULL);
        assert(r.read_bits(1) == 0x1);
    }

    // junk above bit n must be ignored (easy path)
    {
        BitWriter w;
        w.write_bits(0xFF, 4);
        w.write_bits(0x3, 2);
        BitReader r(w.words());
        assert(r.read_bits(4) == 0xF);
        assert(r.read_bits(2) == 0x3);
    }

    // junk above bit n must be ignored (straddle path)
    {
        BitWriter w;
        w.write_bits(0xFFFFFFFFFFFFFFFULL, 60);
        w.write_bits(0x1AB, 8);
        w.write_bits(0x5, 3);
        BitReader r(w.words());
        assert(r.read_bits(60) == 0xFFFFFFFFFFFFFFFULL);
        assert(r.read_bits(8) == 0xAB);
        assert(r.read_bits(3) == 0x5);
    }

    // many single bits (crosses several word boundaries)
    {
        BitWriter w;
        for (int i = 0; i < 200; i++)
            w.write_bits(i & 1, 1);
        BitReader r(w.words());
        for (int i = 0; i < 200; i++)
            assert(r.read_bits(1) == (uint64_t)(i & 1));
    }

    // several full 64 bit words back to back
    {
        BitWriter w;
        for (int i = 0; i < 5; i++)
            w.write_bits(0x0123456789ABCDEFULL + i, 64);
        BitReader r(w.words());
        for (int i = 0; i < 5; i++)
            assert(r.read_bits(64) == 0x0123456789ABCDEFULL + i);
    }

    // exhaustive sweep of every (starting offset, width) pair
    // offset 0...63 bits of prefix, then n = 1...64 bits, then one trailing bit
    {
        for (int offset = 0; offset < 64; offset++)
        {
            for (int n = 1; n <= 64; n++)
            {
                uint64_t prefix = 0x5555555555555555ULL;
                uint64_t val = 0xA5A5A5A5A5A5A5A5ULL;
                BitWriter w;
                w.write_bits(prefix, offset);
                w.write_bits(val, n);
                // checks bits_used afterward
                w.write_bits(1, 1);
                BitReader r(w.words());
                assert(r.read_bits(offset) == (prefix & rmask(offset)));
                uint64_t got = r.read_bits(n);
                if (got != (val & rmask(n)))
                {
                    std::cout << "Failed at offset=" << offset << "\nn=" << n << "\n";
                }
                assert(got == (val & rmask(n)));
                assert(r.read_bits(1) == 1);
            }
        }
    }

    // random widths 1...64, 10,000 values, unmasked inputs
    {
        auto seed = time(0);
        std::mt19937_64 rng(seed);
        std::vector<uint64_t> vals;
        std::vector<int> widths;
        BitWriter w;
        for (int i = 0; i < 10000; i++)
        {
            int n = (int)(rng() % 64) + 1;
            uint64_t v = rng();
            vals.push_back(v & rmask(n));
            widths.push_back(n);
            // unmasked on purpose
            w.write_bits(v, n);
        }
        BitReader r(w.words());
        for (size_t i = 0; i < vals.size(); i++)
        {
            uint64_t got = r.read_bits(widths[i]);
            if (got != vals[i])
            {
                std::cout << "Seed=" << seed << "\n";
                std::cout << "Failed at i=" << i << "\nn=" << widths[i] 
                          << "\nCorrect value=" << vals[i] 
                          << "\nIncorrect value got=" << got << "\n";
            }
            assert(got == vals[i]);
        }
    }

    // n==0 is ignored
    {
        BitWriter w;
        // should not write anything
        w.write_bits(0xFF, 0);
        w.write_bits(0x5, 3);
        w.write_bits(0xFF, 0);
        w.write_bits(0x2, 2);
        BitReader r(w.words());
        assert(r.read_bits(0) == 0);
        assert(r.read_bits(3) == 0x5);
        assert(r.read_bits(0) == 0);
        assert(r.read_bits(2) == 0x2);
        // the zero width writes must not create words
        assert(w.words().size() == 1);
    }

    std::cout << "Bit read and write works\n";
}