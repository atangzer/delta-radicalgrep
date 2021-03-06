#ifndef PIPELINE_KERNEL_H
#define PIPELINE_KERNEL_H

#include <kernel/core/kernel.h>
#include <type_traits>
#include <functional>
#include <kernel/pipeline/driver/driver.h>
#include <boost/container/flat_map.hpp>

namespace llvm { class Value; }

namespace kernel {

const static std::string INITIALIZE_FUNCTION_POINTER_SUFFIX = "_IFP";
const static std::string INITIALIZE_THREAD_LOCAL_FUNCTION_POINTER_SUFFIX = "_ITFP";
const static std::string DO_SEGMENT_FUNCTION_POINTER_SUFFIX = "_SFP";
const static std::string FINALIZE_THREAD_LOCAL_FUNCTION_POINTER_SUFFIX = "_FTIP";
const static std::string FINALIZE_FUNCTION_POINTER_SUFFIX = "_FIP";

class PipelineCompiler;

class PipelineKernel : public Kernel {
    friend class Kernel;
    friend class PipelineCompiler;
    friend class PipelineBuilder;
public:

    static bool classof(const Kernel * const k) {
        return k->getTypeId() == TypeId::Pipeline;
    }

    static bool classof(const void *) { return false; }

public:

    using Scalars = std::vector<Scalar *>;
    using Kernels = std::vector<Kernel *>;

    struct CallBinding {
        const std::string Name;
        llvm::FunctionType * const Type;
        void * const FunctionPointer;
        const Scalars Args;

        llvm::Constant * Callee;

        CallBinding(const std::string Name, llvm::FunctionType * Type, void * FunctionPointer, std::initializer_list<Scalar *> && Args)
        : Name(Name), Type(Type), FunctionPointer(FunctionPointer), Args(Args), Callee(nullptr) { }
    };

    using CallBindings = std::vector<CallBinding>;

    bool hasSignature() const final { return true; }

    bool externallyInitialized() const;

    void setInputStreamSetAt(const unsigned i, StreamSet * const value) final;

    void setOutputStreamSetAt(const unsigned i, StreamSet * const value) final;

    void setInputScalarAt(const unsigned i, Scalar * const value) final;

    void setOutputScalarAt(const unsigned i, Scalar * const value) final;

    llvm::StringRef getSignature() const final {
        return mSignature;
    }

    const unsigned getNumOfThreads() const {
        return mNumOfThreads;
    }

    const unsigned getNumOfSegments() const {
        return mNumOfSegments;
    }

    const Kernels & getKernels() const {
        return mKernels;
    }

    const CallBindings & getCallBindings() const {
        return mCallBindings;
    }

    void addKernelDeclarations(BuilderRef b) final;

    std::unique_ptr<KernelCompiler> instantiateKernelCompiler(BuilderRef b) const noexcept final;

    virtual ~PipelineKernel();

protected:

    PipelineKernel(BaseDriver & driver,
                   std::string && signature,
                   const unsigned numOfThreads, const unsigned numOfSegments,
                   Kernels && kernels, CallBindings && callBindings,
                   Bindings && stream_inputs, Bindings && stream_outputs,
                   Bindings && scalar_inputs, Bindings && scalar_outputs);

    void addFamilyInitializationArgTypes(BuilderRef b, InitArgTypes & argTypes) const final;

    void recursivelyConstructFamilyKernels(BuilderRef b, InitArgs & args, const ParamMap & params) const final;

    void linkExternalMethods(BuilderRef b) final;

    void addAdditionalFunctions(BuilderRef b) final;

    void addInternalProperties(BuilderRef b) final;

    void generateInitializeMethod(BuilderRef b) final;

    void generateInitializeThreadLocalMethod(BuilderRef b) final;

    void generateKernelMethod(BuilderRef b) final;

    void generateFinalizeThreadLocalMethod(BuilderRef b) final;

    void generateFinalizeMethod(BuilderRef b) final;

protected:

    const unsigned                            mNumOfThreads;
    const unsigned                            mNumOfSegments;
    const Kernels                             mKernels;
    CallBindings                              mCallBindings;
    const std::string                         mSignature;
};

}

#endif // PIPELINE_KERNEL_H
