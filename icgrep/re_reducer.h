#ifndef RE_REDUCER_H
#define RE_REDUCER_H

//Regular Expressions
#include "re_re.h"
#include "re_cc.h"
#include "re_name.h"
#include "re_start.h"
#include "re_end.h"
#include "re_seq.h"
#include "re_alt.h"
#include "re_rep.h"

#include <algorithm>
#include <list>
#include <map>

class RE_Reducer
{
public:
    static RE* reduce(RE* re, std::map<std::string, RE*>& re_map);
};

#endif // RE_REDUCER_H
