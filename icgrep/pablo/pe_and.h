/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#ifndef PE_AND_H
#define PE_AND_H

#include <pablo/pabloAST.h>

namespace pablo {

class And : public Variadic {
    friend class PabloBlock;
public:
    static inline bool classof(const PabloAST * e) {
        return e->getClassTypeId() == ClassTypeId::And;
    }
    static inline bool classof(const void *) {
        return false;
    }
    virtual ~And() { }
protected:
    And(const PabloType * const type, PabloAST * expr1, PabloAST * expr2, String * name)
    : Variadic(ClassTypeId::And, type, {expr1, expr2}, name)
    {

    }
    And(const PabloType * const type, const unsigned reserved, String * name)
    : Variadic(ClassTypeId::And, type, reserved, name)
    {

    }
    template<typename iterator>
    And(const PabloType * const type, iterator begin, iterator end, String * name)
    : Variadic(ClassTypeId::And, type, begin, end, name) {

    }
};

}

#endif // PE_AND_H


