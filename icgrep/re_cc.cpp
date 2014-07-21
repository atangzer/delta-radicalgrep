/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include "re_cc.h"

int CC::msCSIidx = 0;

CC::CC()
{
    gensym_name();
}

CC::CC(int codepoint)
{
    gensym_name();
    insert1(codepoint);
}

CC::CC(int lo_codepoint, int hi_codepoint)
{
    gensym_name();
    insert_range(lo_codepoint, hi_codepoint);
}

CC::CC(std::string name, int codepoint)
{
    mName = name;
    insert1(codepoint);
}

CC::CC(std::string name, int lo_codepoint, int hi_codepoint)
{
    mName = name;
    insert_range(lo_codepoint, hi_codepoint);
}

CC::~CC(){}

std::vector<CharSetItem> CC::getItems()
{
    return mSparceCharSet;
}

std::string CC::getName()
{
    return mName;
}

bool CC::is_member(int codepoint)
{
    return is_member_helper(codepoint, mSparceCharSet.size() - 1);
}

bool CC::is_member_helper(int codepoint, int idx)
{
    if (idx == -1)
    {
        return false;
    }
    else
    {
        CharSetItem item = mSparceCharSet.at(idx);

        if (codepoint < item.lo_codepoint)
        {
            return false;
        }
        else if (codepoint > item.hi_codepoint)
        {
            idx--;
            return is_member_helper(codepoint, idx);
        }
        else
        {
            return true;
        }
    }
}

void CC::insert1(int codepoint)
{
    insert_range(codepoint, codepoint);
}

void CC::insert_range(int lo_codepoint, int hi_codepoint)
{
    insert_range_helper(lo_codepoint, hi_codepoint, mSparceCharSet.size() - 1);
}

void CC::insert_range_helper(int lo_codepoint, int hi_codepoint, int idx)
{
    if (idx == -1)
    {
        CharSetItem new_item;
        new_item.lo_codepoint = lo_codepoint;
        new_item.hi_codepoint = hi_codepoint;
        std::vector<CharSetItem>::iterator it;
        it = mSparceCharSet.begin();
        mSparceCharSet.insert(it, new_item);
    }
    else
    {
        CharSetItem item = mSparceCharSet.at(idx);

        if (hi_codepoint < item.lo_codepoint - 1)
        {
            CharSetItem new_item;
            new_item.lo_codepoint = lo_codepoint;
            new_item.hi_codepoint = hi_codepoint;
            std::vector<CharSetItem>::iterator it;
            it = mSparceCharSet.begin();
            mSparceCharSet.insert(it + idx + 1, new_item);
        }
        else if (lo_codepoint > item.hi_codepoint + 1)
        {
            idx--;
            insert_range_helper(lo_codepoint, hi_codepoint, idx);
        }
        else
        {
            int overlap_lo = item.lo_codepoint;
            int overlap_hi = item.hi_codepoint;
            std::vector<CharSetItem>::iterator it;
            it = mSparceCharSet.begin();
            mSparceCharSet.erase(it + idx);
            idx--;
            insert_range_helper(std::min(overlap_lo, lo_codepoint), std::max(overlap_hi, hi_codepoint), idx);
        }
    }
}

void CC::negate_class()
{
    negate_class_helper(mSparceCharSet.size() - 1, 0);
}

void CC::negate_class_helper(int idx, int b)
{
    if (idx == -1)
    {
        if (b <= mUnicodeMax)
        {
            CharSetItem new_item;

            new_item.lo_codepoint = b;
            new_item.hi_codepoint = mUnicodeMax;
            std::vector<CharSetItem>::iterator it;
            it = mSparceCharSet.begin();
            mSparceCharSet.insert(it, new_item);
        }
    }
    else
    {
        CharSetItem item = mSparceCharSet.at(idx);

        if (b < item.lo_codepoint)
        {
            CharSetItem new_item;

            new_item.lo_codepoint = b;
            new_item.hi_codepoint = item.lo_codepoint - 1;
            std::vector<CharSetItem>::iterator it;
            it = mSparceCharSet.begin();
            mSparceCharSet.erase(it + idx);
            mSparceCharSet.insert(it + idx, new_item);
            idx--;
            negate_class_helper(idx, item.hi_codepoint + 1);
        }
        else
        {
            std::vector<CharSetItem>::iterator it;
            it = mSparceCharSet.begin();
            mSparceCharSet.erase(it + idx);
            idx--;
            negate_class_helper(idx, item.hi_codepoint + 1);
        }
    }
}

void CC::remove1(int codepoint)
{
    remove_range(codepoint, codepoint);
}

void CC::remove_range(int lo_codepoint, int hi_codepoint)
{
    remove_range_helper(lo_codepoint, hi_codepoint, mSparceCharSet.size() - 1);
}

void CC::remove_range_helper(int lo_codepoint, int hi_codepoint, int idx)
{
    if (idx != -1)
    {
        CharSetItem item = mSparceCharSet.at(idx);

        if (hi_codepoint < item.lo_codepoint - 1)
        {
            return;
        }
        else if (lo_codepoint > item.hi_codepoint + 1)
        {
            idx--;
            remove_range_helper(lo_codepoint, hi_codepoint, idx);
        }
        else if ((lo_codepoint <= item.lo_codepoint) && (hi_codepoint >= item.hi_codepoint))
        {
            std::vector<CharSetItem>::iterator it;
            it = mSparceCharSet.begin();
            mSparceCharSet.erase(it + idx);
            idx--;
            remove_range_helper(lo_codepoint, hi_codepoint, idx);
        }
        else if (lo_codepoint <= item.lo_codepoint)
        {
            CharSetItem new_item;
            new_item.lo_codepoint = hi_codepoint + 1;
            new_item.hi_codepoint = item.hi_codepoint;
            std::vector<CharSetItem>::iterator it;
            it = mSparceCharSet.begin();
            mSparceCharSet.erase(it + idx);
            mSparceCharSet.insert(it + idx, new_item);
        }
        else if (hi_codepoint >= item.hi_codepoint)
        {
            CharSetItem new_item;
            new_item.lo_codepoint = item.lo_codepoint;
            new_item.hi_codepoint = lo_codepoint - 1;
            std::vector<CharSetItem>::iterator it;
            it = mSparceCharSet.begin();
            mSparceCharSet.erase(it + idx);
            mSparceCharSet.insert(it + idx, new_item);
            idx--;
            remove_range_helper(lo_codepoint, hi_codepoint, idx);
        }
        else
        {
            CharSetItem new_item1;
            new_item1.lo_codepoint = hi_codepoint + 1;
            new_item1.hi_codepoint = item.hi_codepoint;
            CharSetItem new_item2;
            new_item2.lo_codepoint = item.lo_codepoint;
            new_item2.hi_codepoint = lo_codepoint - 1;
            std::vector<CharSetItem>::iterator it;
            it = mSparceCharSet.begin();
            mSparceCharSet.erase(it + idx);
            mSparceCharSet.insert(it + idx, new_item1);
            mSparceCharSet.insert(it + idx, new_item2);
        }
    }
}

void CC::gensym_name()
{
    mName = "lex.CC" + INT2STRING(msCSIidx);
    msCSIidx++;
}

