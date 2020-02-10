#ifndef PARTITION_PROCESSING_LOGIC_HPP
#define PARTITION_PROCESSING_LOGIC_HPP

#include "pipeline_compiler.hpp"

namespace kernel {

//const static std::string PARTITION_LOGICAL_SEGMENT_NUMBER = ".PLS";
//const static std::string PARTITION_ITEM_COUNT_SUFFIX = ".PIC";
//const static std::string PARTITION_FIXED_RATE_SUFFIX = ".PFR";

inline std::string getPartitionSegmentNumberName(const size_t partitionId) {
    return std::to_string(partitionId) + PARTITION_LOGICAL_SEGMENT_NUMBER;
}

inline std::string getPartitionFixedRateName(const size_t partitionId) {
    return std::to_string(partitionId) + PARTITION_FIXED_RATE_SUFFIX;
}

inline std::string getPartitionPortName(const size_t partitionId, const PartitioningGraphEdge & e) {
    std::string tmp;
    raw_string_ostream out(tmp);
    out << partitionId << '.'
        << e.Kernel <<
        ((e.Port.Type == PortType::Input) ? 'I' : 'O') <<
        e.Port.Number <<
        PARTITION_ITEM_COUNT_SUFFIX;
    out.flush();
    return tmp;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief addPartitionItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
inline void PipelineCompiler::addPartitionInputItemCounts(BuilderRef b, const size_t partitionId) const {
    IntegerType * const int64Ty = b->getInt64Ty();
    mTarget->addInternalScalar(int64Ty, getPartitionSegmentNumberName(partitionId));

    bool hasFixedRate = false;
    for (unsigned i = FirstKernel, partitionSize = 0; i <= LastKernel; ++i) {
        if (KernelPartitionId[i] == partitionId) {
            ++partitionSize;
            if (partitionSize > 1) {
                hasFixedRate = true;
                break;
            }
        }
    }

    for (const auto e : make_iterator_range(out_edges(partitionId, mPartitioningGraph))) {
        const auto v = target(e, mPartitioningGraph);
        const PartitioningGraphNode & node = mPartitioningGraph[v];
        if (node.Type == PartitioningGraphNode::Fixed) {
            hasFixedRate = true;
            break;
        }
    }

    if (LLVM_LIKELY(hasFixedRate)) {
        mTarget->addInternalScalar(int64Ty, getPartitionFixedRateName(partitionId));
    }
    for (const auto e : make_iterator_range(in_edges(partitionId, mPartitioningGraph))) {
        mTarget->addInternalScalar(int64Ty, getPartitionPortName(partitionId, mPartitioningGraph[e]));
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief makePartitionEntryPoints
 ** ------------------------------------------------------------------------------------------------------------- */
inline void PipelineCompiler::makePartitionEntryPoints(BuilderRef b) {
    // Make one basic block for each partition and one that will branch into
    // the final exit loop condition.
    mPartitionEntryPoint.resize(PartitionCount + 1);
    for (unsigned i = 0; i <= PartitionCount; ++i) {
        mPartitionEntryPoint[i] = b->CreateBasicBlock("P" + std::to_string(i));
    }    
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief branchToInitialPartition
 ** ------------------------------------------------------------------------------------------------------------- */
inline void PipelineCompiler::branchToInitialPartition(BuilderRef b) {
    assert (KernelPartitionId[FirstKernel] == 0);
    BasicBlock * const entry = mPartitionEntryPoint[0];
    b->CreateBr(entry);
    b->SetInsertPoint(entry);
    mCurrentPartitionId = -1U;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getPartitionExitPoint
 ** ------------------------------------------------------------------------------------------------------------- */
inline BasicBlock * PipelineCompiler::getPartitionExitPoint(BuilderRef b) {
    assert (mKernelIndex >= FirstKernel && mKernelIndex <= LastKernel);
    const auto partitionId = KernelPartitionId[mKernelIndex];
    return mPartitionEntryPoint[partitionId + 1];
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief checkInputDataOnPartitionEntry
 ** ------------------------------------------------------------------------------------------------------------- */
inline void PipelineCompiler::checkInputDataOnPartitionEntry(BuilderRef b) {

    // Only the first node in the partition may have a bounded *input* rate. All other inputs must be countable.
    // Since every kernel in the partition will perform N * C_i strides (where C_i is some constant factor for
    // the i-th kernel), we can trivially calculate the Fixed rates by maintaining a next partition relative stride
    // counter. PartialSum rates, however, must be counted independently.

    // All inputs for the partition must be tested after acquiring the first partition node's lock. No other
    // kernel in the partition is required to test its input. Each kernel, however, is responsible for
    // assessing whether it has sufficient output space for the given input. Although this too could be determined
    // upon entering the partition, other threads may still be actively consuming data from the produced streams;
    // thus we may end up needlessly expanding buffers by performing the test too early.

    assert (mKernelIndex >= FirstKernel && mKernelIndex <= LastKernel);
    const auto partitionId = KernelPartitionId[mKernelIndex];
    if (partitionId == mCurrentPartitionId) {
        return;
    }
    mCurrentPartitionId = partitionId;

    using TypeId = PartitioningGraphNode::TypeId;

//    struct PartitioningGraphNode {
//        enum TypeId {
//            Partition = 0
//            , Bounded
//            , Unknown
//            , Fixed
//            , PartialSum
//            , Greedy
//            , Relative
//        };

//        TypeId Type = TypeId::Partition;
//        unsigned StreamSet = 0; // for non-Fixed rate nodes
//    };

//    struct PartitioningGraphEdge {
//        enum TypeId : unsigned {
//            IOPort = 0
//            , Channel
//            , Reference
//        };

//        TypeId          Type;
//        unsigned        Kernel;
//        StreamSetPort   Port;


//        PartitioningGraphEdge(unsigned kernel, StreamSetPort port) : Type(IOPort), Kernel(kernel), Port(port) { }
//        PartitioningGraphEdge(TypeId type = IOPort, unsigned kernel = 0, StreamSetPort port = StreamSetPort{}) : Type(type), Kernel(kernel), Port(port) { }
//    };


#if 0

    Value * numOfStrides = nullptr;

    for (const auto e : make_iterator_range(in_edges(partitionId, mPartitioningGraph))) {
        const auto u = source(e, mPartitioningGraph);
        const PartitioningGraphNode & node = mPartitioningGraph[u];
        Value * const available = mLocallyAvailableItems[getBufferIndex(node.StreamSet)];
        Value * const closed = isClosed(b, node.StreamSet);

        const PartitioningGraphEdge & E = mPartitioningGraph[e];
        assert (KernelPartitionId[E.Kernel] == partitionId);
        Value * const processed = b->getScalarFieldPtr(getPartitionPortName(partitionId, E));

        Value * const unread = b->CreateSub(available, processed);

        // scan through all consumers of this to see whether there is a lookahead relationship
        unsigned lookAhead = 0;

        for (auto kernel = FirstKernel; kernel <= LastKernel; ++kernel) {
            if (KernelPartitionId[kernel] == partitionId) {




            }
        }









    }

#endif

}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief checkForPartitionExit
 ** ------------------------------------------------------------------------------------------------------------- */
inline void PipelineCompiler::checkForPartitionExit(BuilderRef b) {
    assert (mKernelIndex >= FirstKernel && mKernelIndex <= LastKernel);
    const auto nextPartitionId = KernelPartitionId[mKernelIndex + 1];
    if (nextPartitionId != mCurrentPartitionId) {
        BasicBlock * nextPartition;
        if (LLVM_UNLIKELY(nextPartitionId == -1U)) {
            nextPartition = mPartitionEntryPoint[PartitionCount];
        } else {
            assert (nextPartitionId < PartitionCount);
            nextPartition = mPartitionEntryPoint[nextPartitionId];
        }
        b->CreateBr(nextPartition);
        b->SetInsertPoint(nextPartition);
    }
}

}

#endif
