/*
 *  Copyright (c) 2019 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#pragma once

#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include <boost/optional.hpp>
#include <pablo/parse/error.h>
#include <pablo/parse/pablo_type.h>
#include <pablo/parse/token.h>
#include <kernel/core/attributes.h>

namespace pablo {
namespace parse {


boost::optional<kernel::Attribute> inputParameterAttributeFromString(
    std::shared_ptr<ErrorManager> & em,
    Token * attributeToken,
    boost::optional<uint32_t> amount);

boost::optional<kernel::Attribute> outputParameterAttributeFromString(
    std::shared_ptr<ErrorManager> & em,
    Token * attributeToken,
    boost::optional<uint32_t> amount);

boost::optional<kernel::Attribute> kernelAttributeFromString(
    std::shared_ptr<ErrorManager> & em,
    Token * attributeToken,
    boost::optional<uint32_t> amount);

std::string attributeToString(kernel::Attribute attr);

class PabloKernelSignature {
public:

    struct SignatureBinding {
        std::string                    name;
        PabloType *                    type;
        std::vector<kernel::Attribute> attributes;

        SignatureBinding(std::string name, PabloType * type)
        : name(name), type(type), attributes()
        {}

        SignatureBinding(std::string name, PabloType * type, std::vector<kernel::Attribute> attributes)
        : name(name), type(type), attributes(std::move(attributes))
        {}
    };

    // using SignatureBinding = std::pair<std::string, PabloType *>;
    using SignatureBindings = std::vector<SignatureBinding>;

    PabloKernelSignature(std::string name, SignatureBindings inputBindings, SignatureBindings outputBindings)
    : mName(std::move(name))
    , mInputBindings(std::move(inputBindings))
    , mOutputBindings(std::move(outputBindings))
    {}

    std::string getName() const noexcept {
        return mName;
    }

    SignatureBindings const & getInputBindings() const noexcept {
        return mInputBindings;
    }

    SignatureBindings const & getOutputBindings() const noexcept {
        return mOutputBindings;
    }

    std::string asString() const noexcept;

private:
    std::string         mName;
    SignatureBindings   mInputBindings;
    SignatureBindings   mOutputBindings;
};

} // namespace pablo::parse
} // namespace pablo
