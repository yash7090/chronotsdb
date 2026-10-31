#pragma once
#include <cstdint>
#include <vector>
#include <cassert>

inline uint64_t mask(int n)
{
    return n == 64 ? ~0ULL : (1ULL << n) - 1;
}

class BitWriter
{
public:
    void write_bits(uint64_t value, int n)
    {   if (n == 0) return;
        assert(n >= 0 && n <= 64);
        if (words_.empty() || bits_used == 64)
        {
            words_.push_back(0);
            bits_used = 0;
        }
        value = (value & mask(n));
        int free = 64 - bits_used;
        if (free >= n)
        {
            int shift = 64 - bits_used - n;
            assert(shift >= 0);
            value <<= shift;
            words_[words_.size() - 1] = (words_[words_.size() - 1] | value);
            bits_used += n;
        }
        else if (free < n)
        {
            uint64_t copy = value;
            copy >>= (n - free);
            words_[words_.size() - 1] = (words_[words_.size() - 1] | copy);
            copy = value;
            words_.push_back(0);
            bits_used = 0;
            copy = (copy & ((1ULL << (n - free)) - 1));
            int shift = 64 - bits_used - n + free;
            assert(shift >= 0);
            copy <<= shift;
            words_[words_.size() - 1] = (words_[words_.size() - 1] | copy);
            bits_used = n - free;
        }
    }
    const std::vector<uint64_t> &words() const { return words_; }

private:
    std::vector<uint64_t> words_;
    // bits used in the last word
    int bits_used = 0;
};

class BitReader
{
public:
    BitReader(const std::vector<uint64_t> &words) : words_(words) {}
    uint64_t read_bits(int n)
    {
        if (n == 0) return 0;
        assert(n >= 0 && n <= 64);
        int shift = 64 - bit_pos - n;
        if (shift >= 0)
        {
            uint64_t word = words_[word_idx];
            word >>= shift;
            word = (word & mask(n));
            bit_pos += n;
            if (bit_pos == 64)
            {
                bit_pos = 0;
                word_idx++;
            }
            return word;
        }
        else
        {
            uint64_t word = words_[word_idx];
            word = (word & ((1ULL << (64 - bit_pos)) - 1));
            word <<= n - 64 + bit_pos;
            word_idx++;
            uint64_t word1 = words_[word_idx];
            word = (word | ((word1 >> (128 - n - bit_pos)) & ((1ULL << (n + bit_pos - 64)) - 1)));
            bit_pos = n - 64 + bit_pos;
            return word;
        }
    }

private:
    const std::vector<uint64_t> &words_;
    size_t word_idx = 0;
    // bits already consumed in the current word
    int bit_pos = 0;
};