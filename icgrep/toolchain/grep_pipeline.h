/*
 *  Copyright (c) 2017 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */
#ifndef GREP_PIPELINE_H
#define GREP_PIPELINE_H

#include <stdlib.h>
#include <stdint.h>

namespace re { class RE; }

namespace grep {
    
class MatchAccumulator {
public:
    MatchAccumulator() {};
    virtual void accumulate_match(const size_t lineNum, size_t line_start, size_t line_end) = 0;
};

void accumulate_match_wrapper(intptr_t accum_addr, const size_t lineNum, size_t line_start, size_t line_end);
    
void grepBuffer(re::RE * pattern, const char * buffer, size_t bufferLength, MatchAccumulator * accum);

}

#endif