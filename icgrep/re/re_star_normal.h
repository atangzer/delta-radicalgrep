#ifndef RE_STAR_NORMAL_H
#define RE_STAR_NORMAL_H

#include <re/re_utility.h>

namespace re {

class RE; class Rep;

//A regular expression E is in star normal form if, for each starred
//subexpression H * of E, the following SNF-conditions hold:
//1> The follow(H, last(H)) and first(H) are disjoint.
//2> H is not Nullable.
//
//For example: (a + b)* is the star normal form of (a*b*)* . 
//Both of them have the same Glushkov NFA.
//

// Usage:  RE_Star_Normal().transform(regexp);

class RE_Star_Normal : public RE_Transformer {
    
public:
    RE * transformRep(Rep * rep) override;
};

}

#endif // RE_STAR_NORMAL_H
