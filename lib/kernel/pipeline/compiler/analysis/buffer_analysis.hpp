#ifndef BUFFER_ANALYSIS_HPP
#define BUFFER_ANALYSIS_HPP

#include "../pipeline_compiler.hpp"

// TODO: any buffers that exist only to satisfy the output dependencies are unnecessary.
// We could prune away kernels if none of their outputs are needed but we'd want some
// form of "fake" buffer for output streams in which only some are unnecessary. Making a
// single static thread local buffer thats large enough for one segment.

// TODO: can we "combine" static stream sets that are used together and use fixed offsets
// from the first set? Would this improve data locality or prefetching?

// TODO: generate thread local buffers when we can guarantee all produced data is consumed
// within the same segment "iteration"? We can eliminate synchronization for kernels that
// consume purely local data.

// TODO: if we can prove the liveness of two streams never overlaps, can we reuse the
// memory space.

// TODO: if an external buffer is marked as managed, have it allocate and manage the
// buffer but not deallocate it.

namespace kernel {

inline static unsigned ceil_udiv(const unsigned x, const unsigned y) {
    assert (is_power_2(y));
    return (((x - 1) | (y - 1)) + 1) / y;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief makePipelineBufferGraph
 *
 * Return an acyclic bi-partite graph indicating the I/O relationships between the kernels and their buffers.
 *
 * Ordering: producer -> buffer -> consumer
 ** ------------------------------------------------------------------------------------------------------------- */
BufferGraph PipelineCompiler::makeBufferGraph(BuilderRef b) {

    BufferGraph G(LastStreamSet + 1);

    initializeBufferGraph(G);
    identifyLocalPortIds(G);
    computeDataFlowRates(G);
    identifyLinearBuffers(G);

    // Identify all External I/O buffers
    SmallFlatSet<BufferGraph::vertex_descriptor, 16> IsExternal;

    IsExternal.reserve(out_degree(PipelineInput, G) + in_degree(PipelineOutput, G));
    for (const auto e : make_iterator_range(out_edges(PipelineInput, G))) {
        IsExternal.insert(target(e, G));
    }
    for (const auto e : make_iterator_range(in_edges(PipelineOutput, G))) {
        IsExternal.insert(source(e, G));
    }

    auto internalOrExternal = [&IsExternal](BufferGraph::vertex_descriptor streamSet) -> BufferType {
        if (LLVM_LIKELY(IsExternal.count(streamSet) == 0)) {
            return BufferType::Internal;
        } else {
            return BufferType::External;
        }
    };

    // fill in any known managed buffers
    for (auto kernel = FirstKernel; kernel <= LastKernel; ++kernel) {
        for (const auto e : make_iterator_range(out_edges(kernel, G))) {
            const BufferRateData & producerRate = G[e];
            const Binding & output = producerRate.Binding;
            if (LLVM_UNLIKELY(Kernel::isLocalBuffer(output))) {
                const auto streamSet = target(e, G);
                BufferNode & bn = G[streamSet];
                const auto linear = output.hasAttribute(AttrId::Linear);
                bn.Buffer = new ExternalBuffer(b, output.getType(), linear, 0);
                bn.Type = BufferType::Unowned | internalOrExternal(streamSet);
            }
        }
    }

    // fill in any unmanaged pipeline input buffers
    for (const auto e : make_iterator_range(out_edges(PipelineInput, G))) {
        const auto streamSet = target(e, G);
        BufferNode & bn = G[streamSet];
        if (LLVM_LIKELY(bn.Buffer == nullptr)) {
            const BufferRateData & rate = G[e];
            const Binding & input = rate.Binding;
            bn.Buffer = new ExternalBuffer(b, input.getType(), true, 0);
            bn.Type = BufferType::UnownedExternal;
        }
    }

    // and pipeline output buffers ...
    for (const auto e : make_iterator_range(in_edges(PipelineOutput, G))) {
        const auto streamSet = source(e, G);
        BufferNode & bn = G[streamSet];
        if (LLVM_LIKELY(bn.Buffer == nullptr)) {
            const BufferRateData & rate = G[e];
            const Binding & output = rate.Binding;
            if (LLVM_UNLIKELY(Kernel::isLocalBuffer(output))) continue;
            bn.Buffer = new ExternalBuffer(b, output.getType(), true, 0);
            bn.Type = BufferType::UnownedExternal;
        }
    }

    // Scan through the I/O for each kernel to see if some transitive
    // add relationship imposes an overflow requirement on a buffer

    SmallVector<int, 64> transitiveAdd(LastStreamSet - FirstStreamSet + 1, 0);

    for (auto kernel = FirstKernel; kernel <= LastKernel; ++kernel) {
        int maxK = 0;
        for (const auto e : make_iterator_range(in_edges(kernel, mAddGraph))) {
            maxK = std::max(maxK, mAddGraph[e]);
        }
        for (const auto e : make_iterator_range(out_edges(kernel, mAddGraph))) {
            maxK = std::max(maxK, mAddGraph[e]);
        }
        const auto K = mAddGraph[kernel] + maxK;
        for (const auto e : make_iterator_range(in_edges(kernel, mAddGraph))) {
            const auto streamSet = source(e, mAddGraph) - FirstStreamSet;
            transitiveAdd[streamSet] = std::max(transitiveAdd[streamSet], K);
        }
        for (const auto e : make_iterator_range(out_edges(kernel, mAddGraph))) {
            const auto streamSet = target(e, mAddGraph) - FirstStreamSet;
            transitiveAdd[streamSet] = std::max(transitiveAdd[streamSet], K);
        }
    }


    const auto blockWidth = b->getBitBlockWidth();
    const auto numOfSegments = std::max(mNumOfSegments, mNumOfThreads);

    // then construct the rest
    for (auto streamSet = FirstStreamSet; streamSet <= LastStreamSet; ++streamSet) {

        BufferNode & bn = G[streamSet];
        const auto producerOutput = in_edge(streamSet, G);
        const BufferRateData & producerRate = G[producerOutput];
        const Binding & output = producerRate.Binding;

        bool nonLocal = false;

        // Does this stream cross a partition boundary?
        const auto producer = source(producerOutput, G);
        const auto producerPartitionId = KernelPartitionId[producer];
        for (const auto ce : make_iterator_range(out_edges(streamSet, G))) {
            const auto consumer = target(ce, G);
            if (producerPartitionId != KernelPartitionId[consumer]) {
                nonLocal = true;
                break;
            }
        }


        if (LLVM_LIKELY(bn.Buffer == nullptr)) { // is internal buffer

            // TODO: If we have an open system, then the input rate to this pipeline cannot
            // be bounded a priori. During initialization, we could pass a "suggestion"
            // argument to indicate what the outer pipeline believes its I/O rates will be.

            const BufferType bufferType = internalOrExternal(streamSet);

            bn.Type = bufferType;

            // If this buffer is externally used, we cannot analyze the dataflow rate of
            // external consumers. Default to dynamic for such buffers.

            // Similarly if any internal consumer has a deferred rate, we cannot analyze
            // any consumption rates.

            bool dynamic = nonLocal || (bufferType == BufferType::External) || output.hasAttribute(AttrId::Deferred);

            unsigned lookAhead = 0;
            unsigned lookBehind = 0;
            unsigned reflection = 0;

            const Binding & outputBinding = producerRate.Binding;
            for (const Attribute & attr : outputBinding.getAttributes()) {
                switch (attr.getKind()) {
                    case AttrId::LookBehind:
                        lookBehind = std::max(lookBehind, attr.amount());
                        break;
                    case AttrId::Delayed:
                        reflection = std::max(reflection, attr.amount());
                        break;
                    default: break;
                }
            }

//            const auto pMin = producerRate.Minimum * MinimumNumOfStrides[producer];
            const auto pMax = producerRate.Maximum * MaximumNumOfStrides[producer];

            assert (producerRate.Maximum >= producerRate.Minimum);

            if (producerRate.Minimum< producerRate.Maximum) {
                nonLocal = true;
            }

            // TODO: dataflow analysis must take lookahead/delay into account to permit
            // this optimization.

//            bool isNonLinear = false;
//            if (pMin != pMax) {
//                isNonLinear = true;
//            }

            Rational consumeMin(std::numeric_limits<unsigned>::max());
            Rational consumeMax(std::numeric_limits<unsigned>::min());

            for (const auto ce : make_iterator_range(out_edges(streamSet, G))) {

                const BufferRateData & consumerRate = G[ce];
                const Binding & input = consumerRate.Binding;

                const auto consumer = target(ce, G);

                const auto cMin = consumerRate.Minimum * MinimumNumOfStrides[consumer];
                const auto cMax = consumerRate.Maximum * MaximumNumOfStrides[consumer];

//                if (cMin != cMax) {
//                    isNonLinear = true;
//                }

                // Could we consume less data than we produce?
                if (consumerRate.Minimum < consumerRate.Maximum) {
                    dynamic = true;
                // Or is the data consumption rate unpredictable despite its type?
                } else if (LLVM_UNLIKELY(input.hasAttribute(AttrId::Deferred))) {
                    dynamic = true;
                }
                assert (consumerRate.Maximum >= consumerRate.Minimum);

                consumeMin = std::min(consumeMin, cMin);
                consumeMax = std::max(consumeMax, cMax);

                assert (consumeMax >= consumeMin);

                const Binding & inputBinding = consumerRate.Binding;
                // Get output overflow size
                unsigned tmpLookAhead = 0;
                for (const Attribute & attr : inputBinding.getAttributes()) {
                    switch (attr.getKind()) {
                        case AttrId::LookAhead:
                            tmpLookAhead = std::max(tmpLookAhead, attr.amount());
                            break;
                        case AttrId::LookBehind:
                            lookBehind = std::max(lookBehind, attr.amount());
                            break;
                        default: break;
                    }
                }
                if (consumerRate.Maximum > consumerRate.Minimum) {
                    tmpLookAhead += ceiling(consumerRate.Maximum - consumerRate.Minimum);
                }
                lookAhead = std::max(tmpLookAhead, lookAhead);
            }

            auto copyBack = ceiling(producerRate.Maximum - producerRate.Minimum);

            const auto linear = bn.Linear; // & isNonLinear

            // if this buffer is "stateful", we cannot make it *thread* local
            if (dynamic || lookBehind || reflection || copyBack || lookAhead) {
                nonLocal = true;
            }


            bn.LookBehind = lookBehind;
            bn.LookBehindReflection = reflection;
            bn.CopyBack = copyBack;
            bn.LookAhead = lookAhead;

            // calculate overflow (copyback) and fascimile (copyforward) space
            auto reqOverflow = std::max(copyBack, lookAhead);
            const auto addK = transitiveAdd[streamSet - FirstStreamSet];
            if (addK > 0) {
                reqOverflow = std::max(reqOverflow, static_cast<unsigned>(addK));
            }

            const auto overflowSize = round_up_to(reqOverflow, blockWidth) / blockWidth;
            const auto underflowSize = round_up_to(std::max(lookBehind, reflection), blockWidth) / blockWidth;
            const auto reqSize0 = round_up_to(ceiling(std::max((consumeMax * Rational{2}) - consumeMin, pMax)), blockWidth) / blockWidth;
            const auto reqSize1 = 2 * (overflowSize + underflowSize);
            const auto requiredSize = std::max(reqSize0, reqSize1);

            Type * const baseType = output.getType();

            #ifdef PERMIT_THREAD_LOCAL_BUFFERS
            const auto bufferFactor = nonLocal ? numOfSegments : 1U;
            #else
            const auto bufferFactor = numOfSegments;
            #endif

            const auto bufferSize = requiredSize * bufferFactor;

            // A DynamicBuffer is necessary when we cannot bound the amount of unconsumed data a priori.
            StreamSetBuffer * buffer = nullptr;
            if (dynamic) {
                buffer = new DynamicBuffer(b, baseType, bufferSize, overflowSize, underflowSize, linear, 0U);
            } else {
                buffer = new StaticBuffer(b, baseType, bufferSize, overflowSize, underflowSize, linear, 0U);
            }
            bn.Buffer = buffer;
        }
        bn.NonLocal = nonLocal;
    }

    verifyIOStructure(G);

    return G;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief initializeBufferGraph
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::initializeBufferGraph(BufferGraph & G) const {

    using Graph = adjacency_list<hash_setS, vecS, bidirectionalS, RelationshipGraph::edge_descriptor>;
    using Vertex = graph_traits<Graph>::vertex_descriptor;

    for (auto i = PipelineInput; i <= PipelineOutput; ++i) {
        const RelationshipNode & node = mStreamGraph[i];
        const Kernel * const kernelObj = node.Kernel; assert (kernelObj);

        auto computeBufferRateBounds = [&](const RelationshipType port,
                                           const RelationshipNode & bindingNode,
                                           const unsigned streamSet) {
            assert (bindingNode.Type == RelationshipNode::IsBinding);
            const Binding & binding = bindingNode.Binding;
            const ProcessingRate & rate = binding.getRate();
            Rational lb{rate.getLowerBound()};
            Rational ub{rate.getUpperBound()};
            if (LLVM_UNLIKELY(rate.isGreedy())) {
                if (LLVM_UNLIKELY(port.Type == PortType::Output)) {
                    SmallVector<char, 0> tmp;
                    raw_svector_ostream out(tmp);
                    out << "Greedy rate cannot be applied an output port: "
                        << kernelObj->getName() << "." << binding.getName();
                    report_fatal_error(out.str());
                }
                const auto e = in_edge(streamSet, G);
                const BufferRateData & producerBr = G[e];
                ub = std::max(lb, producerBr.Maximum);
            } else {
                const auto strideLength = kernelObj->getStride();
                if (LLVM_UNLIKELY(rate.isRelative())) {
                    const Binding & ref = getBinding(getReference(i, port));
                    const ProcessingRate & refRate = ref.getRate();
                    lb *= refRate.getLowerBound();
                    ub *= refRate.getUpperBound();
                }
                lb *= strideLength;
                ub *= strideLength;
            }
            return BufferRateData{port, binding, lb, ub};
        };

        // Evaluate the input/output ordering here and ensure that any reference port is stored first.

        const auto numOfInputs = in_degree(i, mStreamGraph);
        const auto numOfPorts = numOfInputs + out_degree(i, mStreamGraph);

        if (numOfPorts == 0) {
            continue;
        }

        Graph E(numOfPorts);

        #ifndef NDEBUG
        RelationshipType prior_in{};
        #endif
        for (auto e : make_iterator_range(in_edges(i, mStreamGraph))) {
            const RelationshipType & port = mStreamGraph[e];
            #ifndef NDEBUG
            assert (prior_in < port);
            prior_in = port;
            #endif
            const auto binding = source(e, mStreamGraph);
            const RelationshipNode & rn = mStreamGraph[binding];
            assert (rn.Type == RelationshipNode::IsBinding);
            E[port.Number] = e;
            if (LLVM_UNLIKELY(in_degree(binding, mStreamGraph) != 1)) {
                for (const auto f : make_iterator_range(in_edges(binding, mStreamGraph))) {
                    const RelationshipType & ref = mStreamGraph[f];
                    if (ref.Reason == ReasonType::Reference) {
                        if (LLVM_UNLIKELY(port.Type == PortType::Output)) {
                            SmallVector<char, 256> tmp;
                            raw_svector_ostream out(tmp);
                            out << "Error: input reference for binding " <<
                                   kernelObj->getName() << "." << rn.Binding.get().getName() <<
                                   " refers to an output stream.";
                            report_fatal_error(out.str());
                        }
                        add_edge(ref.Number, port.Number, E);
                        break;
                    }
                }
            }
        }

        #ifndef NDEBUG
        RelationshipType prior_out{};
        #endif
        for (auto e : make_iterator_range(out_edges(i, mStreamGraph))) {
            const RelationshipType & port = mStreamGraph[e];
            #ifndef NDEBUG
            assert (prior_out < port);
            prior_out = port;
            #endif
            const auto binding = target(e, mStreamGraph);
            const RelationshipNode & rn = mStreamGraph[binding];
            assert (rn.Type == RelationshipNode::IsBinding);
            const auto portNum = port.Number + numOfInputs;
            E[portNum] = e;
            if (LLVM_UNLIKELY(in_degree(binding, mStreamGraph) != 1)) {
                for (const auto f : make_iterator_range(in_edges(binding, mStreamGraph))) {
                    const RelationshipType & ref = mStreamGraph[f];
                    if (ref.Reason == ReasonType::Reference) {
                        auto refPort = ref.Number;
                        if (LLVM_UNLIKELY(ref.Type == PortType::Output)) {
                            refPort += numOfInputs;
                        }
                        add_edge(refPort, portNum, E);
                        break;
                    }
                }
            }
        }

        BitVector V(numOfPorts);
        std::queue<Vertex> Q;

        auto add_edge_if_no_induced_cycle = [&](const Vertex s, const Vertex t) {
            // If s-t exists, skip adding this edge
            if (LLVM_UNLIKELY(edge(s, t, E).second || s == t)) {
                return;
            }

            // If G is a DAG and there is a t-s path, adding s-t will induce a cycle.
            if (in_degree(s, E) > 0) {
                // do a BFS to search for a t-s path
                V.reset();
                assert (Q.empty());
                Q.push(t);
                for (;;) {
                    const auto u = Q.front();
                    Q.pop();
                    for (auto e : make_iterator_range(out_edges(u, E))) {
                        const auto v = target(e, E);
                        if (LLVM_UNLIKELY(v == s)) {
                            // we found a t-s path
                            return;
                        }
                        if (LLVM_LIKELY(!V.test(v))) {
                            V.set(v);
                            Q.push(v);
                        }
                    }
                    if (Q.empty()) {
                        break;
                    }
                }
            }
            add_edge(s, t, E);
        };

        for (unsigned j = 1; j < numOfPorts; ++j) {
            add_edge_if_no_induced_cycle(j - 1, j);
        }

        SmallVector<Graph::vertex_descriptor, 16> ordering;
        ordering.reserve(numOfPorts);
        lexical_ordering(E, ordering);

        for (const auto k : ordering) {
            const auto e = E[k];
            const RelationshipType & port = mStreamGraph[e];
            if (port.Type == PortType::Input) {
                const auto binding = source(e, mStreamGraph);
                const RelationshipNode & rn = mStreamGraph[binding];
                assert (rn.Type == RelationshipNode::IsBinding);
                const auto f = first_in_edge(binding, mStreamGraph);
                assert (mStreamGraph[f].Reason != ReasonType::Reference);
                const auto streamSet = source(f, mStreamGraph);
                assert (mStreamGraph[streamSet].Type == RelationshipNode::IsRelationship);
                assert (isa<StreamSet>(mStreamGraph[streamSet].Relationship));
                add_edge(streamSet, i, computeBufferRateBounds(port, rn, streamSet), G);
            } else {
                const auto binding = target(e, mStreamGraph);
                const RelationshipNode & rn = mStreamGraph[binding];
                assert (rn.Type == RelationshipNode::IsBinding);
                const auto f = out_edge(binding, mStreamGraph);
                assert (mStreamGraph[f].Reason != ReasonType::Reference);
                const auto streamSet = target(f, mStreamGraph);
                assert (mStreamGraph[streamSet].Type == RelationshipNode::IsRelationship);
                assert (isa<StreamSet>(mStreamGraph[streamSet].Relationship));
                add_edge(i, streamSet, computeBufferRateBounds(port, rn, streamSet), G);
            }
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief verifyIOStructure
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::verifyIOStructure(const BufferGraph & G) const {


#if 0

    // verify that the buffer config is valid
    for (unsigned i = FirstStreamSet; i <= LastStreamSet; ++i) {

        const BufferNode & bn = G[i];
        const auto pe = in_edge(i, G);
        const auto producerVertex = source(pe, G);
        const Kernel * const producer = getKernel(producerVertex);
        const BufferRateData & producerRate = G[pe];
        const Binding & output = producerRate.Binding;




        // Type check stream set I/O types.
        Type * const baseType = output.getType();
        for (const auto e : make_iterator_range(out_edges(i, G))) {
            const BufferRateData & consumerRate = G[e];
            const Binding & input = consumerRate.Binding;
            if (LLVM_UNLIKELY(baseType != input.getType())) {
                SmallVector<char, 256> tmp;
                raw_svector_ostream msg(tmp);
                msg << producer->getName() << ':' << output.getName()
                    << " produces a ";
                baseType->print(msg);
                const Kernel * const consumer = getKernel(target(e, G));
                msg << " but "
                    << consumer->getName() << ':' << input.getName()
                    << " expects ";
                input.getType()->print(msg);
                report_fatal_error(msg.str());
            }
        }

        for (const auto ce : make_iterator_range(out_edges(i, G))) {
            const Binding & input = G[ce].Binding;
            if (LLVM_UNLIKELY(requiresLinearAccess(input))) {
                SmallVector<char, 256> tmp;
                raw_svector_ostream out(tmp);
                const auto consumer = target(ce, G);
                out << getKernel(consumer)->getName()
                    << '.' << input.getName()
                    << " requires that "
                    << producer->getName()
                    << '.' << output.getName()
                    << " is a Linear buffer.";
                report_fatal_error(out.str());
            }
        }


    }

#endif

}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief identifyLinearBuffers
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::identifyLinearBuffers(BufferGraph & G) const {

    // Any kernel that is internally synchronized or has a greedy rate input
    // requires that all of its inputs are linear.
    for (auto i = FirstKernel; i <= LastKernel; ++i) {
        const Kernel * const kernelObj = getKernel(i);
        bool mustBeLinear = false;
        if (LLVM_UNLIKELY(kernelObj->hasAttribute(AttrId::InternallySynchronized))) {
            // An internally synchronized kernel requires that all I/O is linear
            for (const auto e : make_iterator_range(out_edges(i, G))) {
                const auto streamSet = target(e, G);
                BufferNode & N = G[streamSet];
                N.Linear = true;
            }
            mustBeLinear = true;
        } else {
            for (const auto e : make_iterator_range(in_edges(i, G))) {
                const BufferRateData & rateData = G[e];
                const Binding & binding = rateData.Binding;
                const ProcessingRate & rate = binding.getRate();
                if (LLVM_UNLIKELY(rate.isGreedy())) {
                    mustBeLinear = true;
                    break;
                }
            }
        }
        if (LLVM_UNLIKELY(mustBeLinear)) {
            for (const auto e : make_iterator_range(in_edges(i, G))) {
                const auto streamSet = source(e, G);
                BufferNode & N = G[streamSet];
                N.Linear = true;
            }
        }
    }

    // If the binding attributes of the producer/consumer(s) of a streamSet indicate
    // that the kernel requires linear input, mark it accordingly.
    for (auto streamSet = FirstStreamSet; streamSet <= LastStreamSet; ++streamSet) {

        const auto binding = in_edge(streamSet, G);
        const BufferRateData & producerRate = G[binding];
        const Binding & output = producerRate.Binding;

        auto requiresLinearAccess = [](const Binding & binding) {
            for (const Attribute & attr : binding.getAttributes()) {
                switch(attr.getKind()) {
                    case AttrId::Linear:
                    case AttrId::Deferred:
                        return true;
                    case AttrId::LookBehind:
                        if (LLVM_UNLIKELY(attr.amount() == 0)) {
                            return true;
                        }
                    default: break;
                }
            }
            return false;
        };

        bool mustBeLinear = false;
        if (LLVM_UNLIKELY(requiresLinearAccess(output))) {
            mustBeLinear = true;
        } else {
            for (const auto binding : make_iterator_range(out_edges(streamSet, G))) {
                const BufferRateData & consumerRate = G[binding];
                const Binding & input = consumerRate.Binding;
                if (LLVM_UNLIKELY(requiresLinearAccess(input))) {
                    mustBeLinear = true;
                    break;
                }
           }
        }
        if (LLVM_UNLIKELY(mustBeLinear)) {
            BufferNode & N = G[streamSet];
            N.Linear = true;
        }
    }
#if 0
    // Any ImplicitPopCount/RegionSelector inputs must be linear to ensure
    // we can easily access all of the rate information.
    for (unsigned i = FirstKernel; i <= LastKernel; ++i) {
        for (const auto e : make_iterator_range(in_edges(i, mStreamGraph))) {
            const RelationshipType & rt = mStreamGraph[e];
            switch (rt.Reason) {
                case ReasonType::ImplicitPopCount:
                case ReasonType::ImplicitRegionSelector:
                    BEGIN_SCOPED_REGION
                    const auto binding = source(e, mStreamGraph);
                    const auto streamSet = parent(binding, mStreamGraph);
                    BufferNode & N = G[streamSet];
                    N.Linear = true;
                    END_SCOPED_REGION
                default: break;
            }
        }
    }
#endif
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief constructInputPortMappings
 ** ------------------------------------------------------------------------------------------------------------- */
BufferPortMap PipelineCompiler::constructInputPortMappings() const {
    size_t n = 0;
    for (auto i = PipelineInput; i <= PipelineOutput; ++i) {
        n += in_degree(i, mBufferGraph);
    }
    BufferPortMap M;
    M.reserve(n);
    for (auto i = PipelineInput; i <= PipelineOutput; ++i) {
        const auto hint = M.nth(M.size());
        for (const auto e : make_iterator_range(in_edges(i, mBufferGraph))) {
            const BufferRateData & input = mBufferGraph[e];
            M.emplace_hint_unique(hint, i, input.Port.Number);
        }
    }
    assert (M.size() == n);
    return M;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief constructOutputPortMappings
 ** ------------------------------------------------------------------------------------------------------------- */
BufferPortMap PipelineCompiler::constructOutputPortMappings() const {
    size_t n = 0;
    for (auto i = PipelineInput; i <= PipelineOutput; ++i) {
        n += out_degree(i, mBufferGraph);
    }
    BufferPortMap M;
    M.reserve(n);
    for (auto i = PipelineInput; i <= PipelineOutput; ++i) {
        const auto hint = M.nth(M.size());
        for (const auto e : make_iterator_range(out_edges(i, mBufferGraph))) {
            const BufferRateData & output = mBufferGraph[e];
            M.emplace_hint_unique(hint, i, output.Port.Number);
        }
    }
    assert (M.size() == n);
    return M;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief mayHaveNonLinearIO
 ** ------------------------------------------------------------------------------------------------------------- */
bool PipelineCompiler::mayHaveNonLinearIO(const size_t kernel) const {

    // If this kernel has I/O that crosses a partition boundary and the
    // buffer itself is not guaranteed to be linear then this kernel
    // may have non-linear I/O. A kernel with non-linear I/O may not be
    // able to execute its full segment without splitting the work across
    // two or more linear sub-segments.

    bool noCountableInput = true;
    for (const auto input : make_iterator_range(in_edges(kernel, mBufferGraph))) {
        const auto streamSet = source(input, mBufferGraph);
        const BufferNode & node = mBufferGraph[streamSet];
        if (node.Linear) {
            // To safely ensure we can determine the maximum number of strides
            // before invoking the kernel, we require at least one countable
            // input. NOTE: the PopCount reference is guaranteed to be linear.
            const BufferRateData & rateData = mBufferGraph[input];
            if (isCountable(rateData.Binding)) {
                noCountableInput = false;
            }
        }
        if (node.NonLocal) {
            return true;
        }
    }
    for (const auto output : make_iterator_range(out_edges(kernel, mBufferGraph))) {
        const auto streamSet = target(output, mBufferGraph);
        const BufferNode & node = mBufferGraph[streamSet];
        if (node.NonLocal || !node.Linear) {
            return true;
        }
    }
    return noCountableInput;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief supportsInternalSynchronization
 ** ------------------------------------------------------------------------------------------------------------- */
bool PipelineCompiler::supportsInternalSynchronization() const {
    if (LLVM_UNLIKELY(mKernel->hasAttribute(AttrId::InternallySynchronized))) {
        if (LLVM_UNLIKELY(mMayHaveNonLinearIO)) {
            SmallVector<char, 256> tmp;
            raw_svector_ostream out(tmp);
            out << "PipelineCompiler error: " <<
                   mKernel->getName() << " is internally synchronized"
                   " but permits non-linear I/O.";
            report_fatal_error(out.str());
        }
        return true;
    }
    return false;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief isBounded
 ** ------------------------------------------------------------------------------------------------------------- */
bool PipelineCompiler::isBounded() const {
    assert (mKernelId >= FirstKernel && mKernelId <= LastKernel);
    for (const auto e : make_iterator_range(in_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];
        const Binding & binding = br.Binding;
        const ProcessingRate & rate = binding.getRate();
        switch (rate.getKind()) {
            case RateId::Bounded:
            case RateId::Fixed:
            case RateId::PartialSum:
                return true;
            case RateId::Greedy:
                if (rate.getLowerBound() > Rational{0, 1}) {
                    return true;
                }
            default: break;
        }
    }
    return false;
}


}

#endif
