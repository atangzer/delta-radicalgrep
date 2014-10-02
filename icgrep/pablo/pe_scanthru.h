/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#ifndef PS_SCANTHRU_H
#define PS_SCANTHRU_H

#include <pablo/pe_pabloe.h>

namespace pablo {

class ScanThru : public  PabloE {
    friend PabloE * make_scanthru(PabloE *, PabloE *);
public:
    static inline bool classof(const PabloE * e) {
        return e->getClassTypeId() == ClassTypeId::ScanThru;
    }
    static inline bool classof(const void *) {
        return false;
    }
    ScanThru(PabloE * from, PabloE * thru)
    : PabloE(ClassTypeId::ScanThru)
    , mScanFrom(from)
    , mScanThru(thru)
    {

    }

    virtual ~ScanThru() {
        delete mScanFrom;
        delete mScanThru;
    }

    PabloE * getScanFrom() const {
        return mScanFrom;
    }

    PabloE * getScanThru() const {
        return mScanThru;
    }
private:
    PabloE * const mScanFrom;
    PabloE * const mScanThru;
};

inline PabloE * make_scanthru(PabloE * from, PabloE * thru) {
    return new ScanThru(from, thru);
}

}

#endif // PS_SCANTHRU_H



