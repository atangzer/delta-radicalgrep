#include <kernel/pipeline/pipeline_kernel.h>
#include "compiler/pipeline_compiler.hpp"
#include <llvm/IR/Function.h>

// NOTE: the pipeline kernel is primarily a proxy for the pipeline compiler. Ideally, by making some kernels
// a "family", the pipeline kernel will be compiled once for the lifetime of a program. Thus we can avoid even
// constructing any data structures for the pipeline in normal usage.

namespace kernel {

#define COMPILER (static_cast<PipelineCompiler *>(b->getCompiler()))

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief addInternalKernelProperties
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::addInternalProperties(BuilderRef b) {
    COMPILER->generateImplicitKernels(b);
    COMPILER->addPipelineKernelProperties(b);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief generateInitializeMethod
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::generateInitializeMethod(BuilderRef b) {
    COMPILER->generateInitializeMethod(b);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief generateInitializeThreadLocalMethod
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::generateInitializeThreadLocalMethod(BuilderRef b) {
    COMPILER->generateInitializeThreadLocalMethod(b);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief generateKernelMethod
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::generateKernelMethod(BuilderRef b) {
    COMPILER->generateKernelMethod(b);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief generateFinalizeMethod
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::generateFinalizeMethod(BuilderRef b) {
    COMPILER->generateFinalizeMethod(b);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief generateFinalizeThreadLocalMethod
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::generateFinalizeThreadLocalMethod(BuilderRef b) {
    COMPILER->generateFinalizeThreadLocalMethod(b);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief addKernelDeclarations
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::addKernelDeclarations(BuilderRef b) {
    for (const auto & k : mKernels) {
        k->addKernelDeclarations(b);
    }
    Kernel::addKernelDeclarations(b);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief linkExternalMethods
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::linkExternalMethods(BuilderRef b) {
    for (const auto & k : mKernels) {
        k->linkExternalMethods(b);
    }
    for (CallBinding & call : mCallBindings) {
        call.Callee = b->LinkFunction(call.Name, call.Type, call.FunctionPointer);
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief addAdditionalFunctions
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::addAdditionalFunctions(BuilderRef b) {
    if (!externallyInitialized()) {
        addOrDeclareMainFunction(b, Kernel::AddExternal);
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief containsKernelFamilies
 ** ------------------------------------------------------------------------------------------------------------- */
bool PipelineKernel::externallyInitialized() const {
    if (LLVM_UNLIKELY(hasFamilyName())) {
        return true;
    }
    for (Kernel * k : mKernels) {
        if (k->externallyInitialized()) {
            return true;
        }
    }
    return false;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief addFamilyInitializationArgTypes
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::addFamilyInitializationArgTypes(BuilderRef b, InitArgTypes & argTypes) const {
    unsigned n = 0;
    for (const Kernel * kernel : mKernels) {
        if (kernel->externallyInitialized()) {
            if (LLVM_LIKELY(kernel->isStateful())) {
                n += 1;
            }
            if (LLVM_LIKELY(kernel->hasFamilyName())) {
                unsigned m = 2;
                if (LLVM_UNLIKELY(kernel->hasThreadLocal())) {
                    m = 4;
                }
                n += m;
            }
        }
    }
    if (LLVM_LIKELY(n > 0)) {
        argTypes.append(n, b->getVoidPtrTy());
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief recursivelyConstructFamilyKernels
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::recursivelyConstructFamilyKernels(BuilderRef b, InitArgs & args, const ParamMap & params) const {
    for (const Kernel * const kernel : mKernels) {
        if (LLVM_UNLIKELY(kernel->externallyInitialized())) {
            kernel->constructFamilyKernels(b, args, params);
        }
    }

}

#define JOIN3(X,Y,Z) BOOST_JOIN(X,BOOST_JOIN(Y,Z))

#define REPLACE_INTERNAL_KERNEL_BINDINGS(BindingType) \
    const auto * const from = JOIN3(m, BindingType, s)[i].getRelationship(); \
    for (auto * K : mKernels) { \
        const auto & B = K->JOIN3(get, BindingType, Bindings)(); \
        for (unsigned j = 0; j < B.size(); ++j) { \
            if (LLVM_UNLIKELY(B[j].getRelationship() == from)) { \
                K->JOIN3(set, BindingType, At)(j, value); } } } \
    JOIN3(m, BindingType, s)[i].setRelationship(value);

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief setInputStreamSetAt
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::setInputStreamSetAt(const unsigned i, StreamSet * const value) {
    REPLACE_INTERNAL_KERNEL_BINDINGS(InputStreamSet);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief setOutputStreamSetAt
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::setOutputStreamSetAt(const unsigned i, StreamSet * const value) {
    REPLACE_INTERNAL_KERNEL_BINDINGS(OutputStreamSet);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief setInputScalarAt
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::setInputScalarAt(const unsigned i, Scalar * const value) {
    REPLACE_INTERNAL_KERNEL_BINDINGS(InputScalar);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief setOutputScalarAt
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineKernel::setOutputScalarAt(const unsigned i, Scalar * const value) {
    REPLACE_INTERNAL_KERNEL_BINDINGS(OutputScalar);
}

#undef JOIN3
#undef REPLACE_INTERNAL_KERNEL_BINDINGS

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief instantiateKernelCompiler
 ** ------------------------------------------------------------------------------------------------------------- */
std::unique_ptr<KernelCompiler> PipelineKernel::instantiateKernelCompiler(BuilderRef b) const noexcept {
    return llvm::make_unique<PipelineCompiler>(b, const_cast<PipelineKernel *>(this));
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief constructor
 ** ------------------------------------------------------------------------------------------------------------- */
PipelineKernel::PipelineKernel(BaseDriver & driver,
                               std::string && signature,
                               const unsigned numOfThreads, const unsigned numOfSegments,
                               Kernels && kernels, CallBindings && callBindings,
                               Bindings && stream_inputs, Bindings && stream_outputs,
                               Bindings && scalar_inputs, Bindings && scalar_outputs)
: Kernel(driver.getBuilder(), TypeId::Pipeline,
         [&] () {
             std::string tmp;
             tmp.reserve(32);
             llvm::raw_string_ostream name(tmp);
             name << 'P' << numOfThreads
                  << 'B' << numOfSegments
                  << '_' << Kernel::getStringHash(signature);
             name.flush();
             return tmp;
         } (),
         std::move(stream_inputs), std::move(stream_outputs),
         std::move(scalar_inputs), std::move(scalar_outputs), {})
, mNumOfThreads(numOfThreads)
, mNumOfSegments(numOfSegments)
, mKernels(std::move(kernels))
, mCallBindings(std::move(callBindings))
, mSignature(std::move(signature)) {

}

PipelineKernel::~PipelineKernel() {

}

}
