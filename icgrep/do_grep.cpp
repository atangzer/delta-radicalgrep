/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include "icgrep.h"
#include "do_grep.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdexcept>

#include "include/simd-lib/carryQ.hpp"
#include "include/simd-lib/pabloSupport.hpp"
#include "include/simd-lib/s2p.hpp"
#include "include/simd-lib/buffer.hpp"

#include <llvm/Support/raw_os_ostream.h>

// mmap system
#ifdef USE_BOOST_MMAP
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
using namespace boost::iostreams;
using namespace boost::filesystem;
#else
#include <sys/mman.h>
#endif
#include <fcntl.h>


#define BUFFER_SEGMENTS 15
#define BUFFER_SIZE (BUFFER_SEGMENTS * SEGMENT_SIZE)

//
// Write matched lines from a buffer to an output file, given segment
// scanners for line ends and matches (where matches are a subset of line ends).
// The buffer pointer must point to the first byte of the segment
// corresponding to the scanner indexes.   The first_line_start is the
// start position of the first line relative to the buffer start position.
// It must be zero or negative;  if negative, the buffer must permit negative
// indexing so that the lineup to the buffer start position can also be printed.
// The start position of the final line in the processed segment is returned.
//

ssize_t GrepExecutor::write_matches(llvm::raw_ostream & out, const char * buffer, ssize_t line_start) {

    ssize_t match_pos;
    ssize_t line_end;
    while (mMatch_scanner.has_next()) {
        match_pos = mMatch_scanner.scan_to_next();
        // If we found a match, it must be at a line end.
        while (true) {
            line_end = mLineBreak_scanner.scan_to_next();
            if (line_end >= match_pos) {
                break;
            }
            line_start = line_end + 1;
            mLineNum++;
        }
        assert (buffer + line_end < mFileBuffer + mFileSize);
        if (mShowFileNameOption) {
            out << mFileName << ':';
        }
        if (mShowLineNumberingOption) {
            out << mLineNum << ":";
        }
        if ((buffer[line_start] == 0xA) && (line_start != line_end)) {
            // The LF of a CRLF.  Really the end of the last line.
            line_start++;
        }
        unsigned char end_byte = (unsigned char)buffer[line_end];
        if (mNormalizeLineBreaksOption) {
            if (end_byte == 0x85) {
                // Line terminated with NEL, on the second byte.  Back up 1.
                line_end--;
            } else if (end_byte > 0xD) {
                // Line terminated with PS or LS, on the third byte.  Back up 2.
                line_end -= 2;
            }
            out.write(&buffer[line_start], line_end - line_start);
            out << '\n';
        }
        else {
            if (end_byte == 0x0) {
                // This must be a sentinel byte position at the end of file.
                // Do not write it.
                line_end--;
            } else if (end_byte == 0x0D) {
                // Check for line_end on first byte of CRLF;  note that to safely
                // access past line_end, even at the end of buffer, we require the
                // MMAP_SENTINEL_BYTES >= 1.
                if (buffer[line_end + 1] == 0x0A) {
                    // Found CRLF; preserve both bytes.
                    line_end++;
                }
            }
            out.write(&buffer[line_start], line_end - line_start + 1);
        }
        line_start = line_end + 1;
        mLineNum++;
    }
    while(mLineBreak_scanner.has_next()) {
        line_end = mLineBreak_scanner.scan_to_next();
        line_start = line_end+1;
        mLineNum++;
    }
    return line_start;
}

bool GrepExecutor::finalLineIsUnterminated() const {
    if (mFileSize == 0) return false;
    unsigned char end_byte = static_cast<unsigned char>(mFileBuffer[mFileSize-1]);
    // LF through CR are line break characters
    if ((end_byte >= 0xA) && (end_byte <= 0xD)) return false;
    // Other line breaks require at least two bytes.
    if (mFileSize == 1) return true;
    // NEL
    unsigned char penult_byte = static_cast<unsigned char>(mFileBuffer[mFileSize-2]);
    if ((end_byte == 0x85) && (penult_byte == 0xC2)) return false;
    if (mFileSize == 2) return true;
    // LS and PS
    if ((end_byte < 0xA8) || (end_byte > 0xA9)) return true;
    return (static_cast<unsigned char>(mFileBuffer[mFileSize-3]) != 0xE2) || (penult_byte != 0x80);
}

void GrepExecutor::doGrep(const std::string & fileName) {

    Basis_bits basis_bits;
    BitBlock match_vector = simd<1>::constant<0>();
    size_t match_count = 0;
    size_t chars_avail = 0;
    ssize_t line_start = 0;

    mFileName = fileName;
    mLineNum = 1;

#ifdef USE_BOOST_MMAP
    const path file(mFileName);
    if (exists(file)) {
        if (is_directory(file)) {
            return;
        }
    } else {
        std::cerr << "Error: cannot open " << mFileName << " for processing. Skipped.\n";
        return;
    }

    mFileSize = file_size(file);
    mapped_file mFile;
    try {
        mFile.open(mFileName, mapped_file::priv, mFileSize, 0);
    } catch (std::ios_base::failure e) {
        std::cerr << "Error: Boost mmap " << e.what() << std::endl;
        return;
    }
    mFileBuffer = mFile.data();
#else
    struct stat infile_sb;
    const int fdSrc = open(mFileName.c_str(), O_RDONLY);
    if (fdSrc == -1) {
        std::cerr << "Error: cannot open " << mFileName << " for processing. Skipped.\n";
        return;
    }
    if (fstat(fdSrc, &infile_sb) == -1) {
        std::cerr << "Error: cannot stat " << mFileName << " for processing. Skipped.\n";
        close (fdSrc);
        return;
    }
    if (S_ISDIR(infile_sb.st_mode)) {
        close (fdSrc);
        return;
    }
    mFileSize = infile_sb.st_size;
    mFileBuffer = (char *) mmap(NULL, mFileSize, PROT_READ, MAP_PRIVATE, fdSrc, 0);
    if (mFileBuffer == MAP_FAILED) {
        if (errno ==  ENOMEM) {
            std::cerr << "Error:  mmap of " << mFileName << " failed: out of memory\n";
        }
        else {
            std::cerr << "Error: mmap of " << mFileName << " failed with errno " << errno << ". Skipped.\n";
        }
        return;
    }
#endif
    size_t segment = 0;
    chars_avail = mFileSize;

    llvm::raw_os_ostream out(std::cout);
    //////////////////////////////////////////////////////////////////////////////////////////
    // Full Segments
    //////////////////////////////////////////////////////////////////////////////////////////

    while (chars_avail >= SEGMENT_SIZE) {

        mLineBreak_scanner.init();
        mMatch_scanner.init();

        for (size_t blk = 0; blk != SEGMENT_BLOCKS; ++blk) {
            s2p_do_block(reinterpret_cast<BytePack *>(mFileBuffer + (blk * BLOCK_SIZE) + (segment * SEGMENT_SIZE)), basis_bits);
            Output output;
            mProcessBlockFcn(basis_bits, output);

            mMatch_scanner.load_block(output.matches, blk);
            mLineBreak_scanner.load_block(output.LF, blk);

            if (mCountOnlyOption) {
                if (bitblock::any(output.matches)) {
                    if (bitblock::any(simd_and(match_vector, output.matches))) {
                        match_count += bitblock::popcount(match_vector);
                        match_vector = output.matches;
                    } else {
                        match_vector = simd_or(match_vector, output.matches);
                    }
                }
            }
        }

        if (!mCountOnlyOption) {
            line_start = write_matches(out, mFileBuffer + (segment * SEGMENT_SIZE), line_start);
        }
        segment++;
        line_start -= SEGMENT_SIZE;  /* Will be negative offset for use within next segment. */
        chars_avail -= SEGMENT_SIZE;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // For the Final Partial Segment.
    //////////////////////////////////////////////////////////////////////////////////////////

    size_t remaining = chars_avail;
    size_t blk = 0;

    mLineBreak_scanner.init();
    mMatch_scanner.init();

    /* Full Blocks */
    for (; remaining >= BLOCK_SIZE; remaining -= BLOCK_SIZE, ++blk) {
        s2p_do_block(reinterpret_cast<BytePack *>(mFileBuffer + (blk * BLOCK_SIZE) + (segment * SEGMENT_SIZE)), basis_bits);
        Output output;
        mProcessBlockFcn(basis_bits, output);

        mLineBreak_scanner.load_block(output.LF, blk);
        mMatch_scanner.load_block(output.matches, blk);
        if (mCountOnlyOption) {
            if (bitblock::any(output.matches)) {
                if (bitblock::any(simd_and(match_vector, output.matches))) {
                    match_count += bitblock::popcount(match_vector);
                    match_vector = output.matches;
                } else {
                    match_vector = simd_or(match_vector, output.matches);
                }
            }
        }
    }

    //Final Partial Block (may be empty, but there could be carries pending).
    
    const auto EOF_mask = bitblock::srl(simd<1>::constant<1>(), convert(BLOCK_SIZE - remaining));
    
    s2p_do_final_block(reinterpret_cast<BytePack *>(mFileBuffer + (blk * BLOCK_SIZE) + (segment * SEGMENT_SIZE)), basis_bits, EOF_mask);

    if (finalLineIsUnterminated()) {
        // Add a LF at the EOF position
        BitBlock EOF_pos = simd_not(simd_or(bitblock::slli<1>(simd_not(EOF_mask)), EOF_mask));
        //  LF = 00001010  (bits 4 and 6 set).
        basis_bits.bit_4 = simd_or(basis_bits.bit_4, EOF_pos);
        basis_bits.bit_6 = simd_or(basis_bits.bit_6, EOF_pos);
    }
    
    Output output;
    mProcessBlockFcn(basis_bits, output);

    if (mCountOnlyOption) {
        match_count += bitblock::popcount(match_vector);
        if (bitblock::any(output.matches)) {
            match_count += bitblock::popcount(output.matches);
        }
        if (mShowFileNameOption) {
            out << mFileName << ':';
        }
        out << match_count << '\n';
    } else {
        mLineBreak_scanner.load_block(output.LF, blk);
        mMatch_scanner.load_block(output.matches, blk);
        while (++blk < SEGMENT_BLOCKS) {
            mLineBreak_scanner.load_block(simd<1>::constant<0>(), blk);
            mMatch_scanner.load_block(simd<1>::constant<0>(), blk);
        }
        line_start = write_matches(out, mFileBuffer + (segment * SEGMENT_SIZE), line_start);
    }
#ifdef USE_BOOST_MMAP
    mFile.close();
#else
    munmap((void *)mFileBuffer, mFileSize);
    close(fdSrc);
#endif   
}
