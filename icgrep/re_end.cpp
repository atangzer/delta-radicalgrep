/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include "re_end.h"

End::End()
{
    mCC = new CC();
}

CC* End::getCC()
{
    return mCC;
}

End::~End(){}
