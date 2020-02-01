#ifndef DATAFLOW_ANALYSIS_HPP
#define DATAFLOW_ANALYSIS_HPP

#include "../pipeline_compiler.hpp"

namespace kernel {


/** ------------------------------------------------------------------------------------------------------------- *
 * @brief identifyHardPartitionConstraints
 ** ------------------------------------------------------------------------------------------------------------- */
PartitionConstraintGraph PipelineCompiler::identifyHardPartitionConstraints(BufferGraph & G) const {

    PartitionConstraintGraph H(PartitionCount);

    const auto firstKernel = out_degree(PipelineInput, G) == 0 ? FirstKernel : PipelineInput;
    const auto lastKernel = in_degree(PipelineOutput, G) == 0 ? LastKernel : PipelineOutput;

    for (auto kernel = firstKernel; kernel <= lastKernel; ++kernel) {

        const auto partitionId = KernelPartitionId[kernel];

        for (const auto input : make_iterator_range(in_edges(kernel, G))) {
            const auto buffer = source(input, G);
            const auto producer = parent(buffer, G);
            const auto producerPartitionId = KernelPartitionId[producer];
            assert (producerPartitionId <= partitionId);
            if (producerPartitionId != partitionId) {
                const BufferRateData & inputRate = G[input];
                const Binding & binding = inputRate.Binding;
                const ProcessingRate & rate = binding.getRate();
                if (LLVM_UNLIKELY(!rate.isFixed())) {
                    add_edge(producerPartitionId, partitionId, H);
                }
            }
        }

        for (const auto output : make_iterator_range(out_edges(kernel, G))) {
            const auto buffer = target(output, G);
            for (const auto data : make_iterator_range(out_edges(buffer, G))) {
                const auto consumer = target(data, G);
                const auto consumerPartitionId = KernelPartitionId[consumer];
                assert (consumerPartitionId >= partitionId);
                if (consumerPartitionId != partitionId) {
                    const BufferRateData & outputRate = G[output];
                    const Binding & binding = outputRate.Binding;
                    const ProcessingRate & rate = binding.getRate();
                    if (LLVM_UNLIKELY(!rate.isFixed())) {
                        add_edge(partitionId, consumerPartitionId, H);
                    }
                }
            }
        }
    }

    return H;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief identifyHardPartitionConstraints
 ** ------------------------------------------------------------------------------------------------------------- */
LengthConstraintGraph PipelineCompiler::identifyLengthEqualityAssertions(BufferGraph & G) const {

    LengthConstraintGraph H(LastStreamSet + 1);

    if (mLengthAssertions.empty()) {
        return H;
    }

    flat_map<const StreamSet *, unsigned> M;

    for (auto streamSet = FirstStreamSet; streamSet <= LastStreamSet; ++streamSet) {
        const auto output = in_edge(streamSet, G);
        const BufferRateData & br = G[output];
        const Binding & binding = br.Binding;
        M.emplace(cast<StreamSet>(binding.getRelationship()), streamSet);
    }

    auto offset = [&](const StreamSet * streamSet) {
        const auto f = M.find(streamSet);
        assert (f != M.end());
        return f->second;
    };

    for (const LengthAssertion & la : mLengthAssertions) {
        add_edge(offset(la[0]), offset(la[1]), H);
    }

    return H;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief computeParitionDataFlowRates
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::computeDataFlowRates(BufferGraph & G) {

    using Bound = std::array<Rational, 2>;

    const auto cfg = Z3_mk_config();
    Z3_set_param_value(cfg, "model", "true");
    Z3_set_param_value(cfg, "proof", "false");
    const auto ctx = Z3_mk_context(cfg);
    Z3_del_config(cfg);
    const auto solver = Z3_mk_solver(ctx);
    Z3_solver_inc_ref(ctx, solver);

    const auto varType = Z3_mk_real_sort(ctx);

    auto constant = [&](const Rational value) {
        return Z3_mk_real(ctx, value.numerator(), value.denominator());
    };

    const auto ONE = constant(1);

    auto average = [&](const BufferRateData & rate) {
        return constant((rate.Minimum + rate.Maximum) * Rational{1, 2});
    };

    auto maximum = [&](const BufferRateData & rate) {
        return constant(rate.Maximum);
    };

    auto minimum = [&](const BufferRateData & rate) {
        return constant(rate.Minimum);
    };

    auto free_variable = [&]() {
        auto v = Z3_mk_fresh_const(ctx, nullptr, varType);
        auto c1 = Z3_mk_ge(ctx, v, ONE);
        Z3_solver_assert(ctx, solver, c1);
        return v;
    };

    auto lower_bounded_variable = [&](const BufferRateData & rate) {
        auto v = Z3_mk_fresh_const(ctx, nullptr, varType);
        auto c1 = Z3_mk_ge(ctx, v, minimum(rate));
        Z3_solver_assert(ctx, solver, c1);
        return v;
    };

    auto bounded_variable = [&](const BufferRateData & rate) {
        assert (rate.Minimum < rate.Maximum);
        auto v = Z3_mk_fresh_const(ctx, nullptr, varType);
        auto c1 = Z3_mk_ge(ctx, v, minimum(rate));
        Z3_solver_assert(ctx, solver, c1);
        auto c2 = Z3_mk_le(ctx, v, maximum(rate));
        Z3_solver_assert(ctx, solver, c2);
        return v;
    };

    auto multiply =[&](Z3_ast X, Z3_ast Y) {
        Z3_ast args[2] = { X, Y };
        return Z3_mk_mul(ctx, 2, args);
    };

    const auto firstKernel = out_degree(PipelineInput, G) == 0 ? FirstKernel : PipelineInput;
    const auto lastKernel = in_degree(PipelineOutput, G) == 0 ? LastKernel : PipelineOutput;

    std::vector<Z3_ast> VarList(LastStreamSet + 1);

    for (auto kernel = firstKernel; kernel <= lastKernel; ++kernel) {
        // Source kernels always perform exactly one iteration
        if (in_degree(kernel, G) == 0) {
            VarList[kernel] = ONE;
        } else {
            VarList[kernel] = free_variable();
        }
    }

    enum {
        LowerBound = 0,
        UpperBound = 1,
        Common = 2
    };

    const auto H = identifyHardPartitionConstraints(G);

    auto crossesVariableRatePartitionBoundary =[&H](const unsigned p1, const unsigned p2) {
        return edge(p1, p2, H).second;
    };

    std::array<std::vector<Z3_ast>, 3> assumptions;

    for (auto kernel = firstKernel; kernel <= lastKernel; ++kernel) {

        const auto stridesPerSegmentVar = VarList[kernel];
        const auto partitionId = KernelPartitionId[kernel];

        unsigned numOfGreedyRates = 0;

        for (const auto input : make_iterator_range(in_edges(kernel, G))) {
            const auto buffer = source(input, G);

            const BufferRateData & inputRate = G[input];
            const Binding & binding = inputRate.Binding;
            const ProcessingRate & rate = binding.getRate();

            const auto producedRate = VarList[buffer]; assert (producedRate);

            if (LLVM_LIKELY(rate.isFixed())) {

                const auto fixedRateVal = minimum(inputRate);
                const auto consumedRate = multiply(stridesPerSegmentVar, fixedRateVal);
                // To consume any data, we want to guarantee at least one full stride of work.
                const auto atLeastOneStride = Z3_mk_ge(ctx, consumedRate, fixedRateVal);
                assumptions[Common].push_back(atLeastOneStride);
                // A partition-local stream must *always* consume everything but streams that
                // cross a partition simply need to produce enough to satisfy its consumer(s)
                const auto consumeEverything = Z3_mk_eq(ctx, producedRate, consumedRate);
                const auto producer = parent(buffer, G);

                if (crossesVariableRatePartitionBoundary(KernelPartitionId[producer], partitionId)) {
                    const auto produceEnough = Z3_mk_ge(ctx, producedRate, consumedRate);
                    Z3_solver_assert(ctx, solver, produceEnough);
                    // Assume we can consume all of the data
                    assumptions[Common].push_back(consumeEverything);
                } else {
                    Z3_solver_assert(ctx, solver, consumeEverything);
                }
            } else if (LLVM_UNLIKELY(rate.isGreedy())) {
                ++numOfGreedyRates;
            } else {

                const auto inputRateVar = bounded_variable(inputRate);
                const auto consumedRate = multiply(stridesPerSegmentVar, inputRateVar);
                const auto produceEnough = Z3_mk_ge(ctx, producedRate, consumedRate);
                Z3_solver_assert(ctx, solver, produceEnough);

                // To consume any data, we want to guarantee at least one full stride of work;
                // however we can only state that only Bounded rates will block on less than
                // this amount.
                const auto atLeastOneStride = Z3_mk_ge(ctx, consumedRate, maximum(inputRate));
                if (rate.isBounded()) {
                    Z3_solver_assert(ctx, solver, atLeastOneStride);
                } else {
                    assumptions[Common].push_back(atLeastOneStride);
                }

                // Since we are trying to determine the lower and upper bound on the number
                // of strides per segment, to determine the lower bound we assume that a
                // kernel consumes the maximum amount of data but produces the minimum.
                // The upper bound is similar except we consume the minimum and produce
                // the maximum.

                const auto avgInputRate = average(inputRate);

                assumptions[LowerBound].push_back(Z3_mk_ge(ctx, inputRateVar, avgInputRate));
                assumptions[LowerBound].push_back(Z3_mk_eq(ctx, inputRateVar, maximum(inputRate)));

                assumptions[UpperBound].push_back(Z3_mk_le(ctx, inputRateVar, avgInputRate));
                assumptions[UpperBound].push_back(Z3_mk_eq(ctx, consumedRate, minimum(inputRate)));
            }
        }

        // Any kernel with all greedy rates must exhaust its input in a single iteration.
        if (LLVM_UNLIKELY(numOfGreedyRates == in_degree(kernel, G))) {
            const auto constraint = Z3_mk_eq(ctx, stridesPerSegmentVar, ONE);
            Z3_solver_assert(ctx, solver, constraint);
        }

        for (const auto output : make_iterator_range(out_edges(kernel, G))) {
            const BufferRateData & outputRate = G[output];
            const Binding & binding = outputRate.Binding;
            const ProcessingRate & rate = binding.getRate();
            const auto buffer = target(output, G);
            if (LLVM_LIKELY(rate.isFixed())) {
                const auto outputRateVar = maximum(outputRate);
                const auto producedRate = multiply(stridesPerSegmentVar, outputRateVar);
                VarList[buffer] = producedRate;
            } else if (LLVM_UNLIKELY(rate.isUnknown())) {
                // TODO: is there a better way to handle unknown outputs? This
                // free variable represents the ideal amount of data to transfer
                // to subsequent kernels but that isn't very meaningful here.
                VarList[buffer] = lower_bounded_variable(outputRate);
            } else {
                const auto outputRateVar = bounded_variable(outputRate);
                const auto producedRate = multiply(stridesPerSegmentVar, outputRateVar);
                VarList[buffer] = producedRate;
                // Like above, when calculating the lower bound of the number of kernel strides,
                // we assume we've produced the minimum amount of data and for the upper bound,
                // the maximum.

                const auto avgOutputRate = average(outputRate);
                assumptions[LowerBound].push_back(Z3_mk_le(ctx, outputRateVar, avgOutputRate));
                assumptions[LowerBound].push_back(Z3_mk_eq(ctx, producedRate, minimum(outputRate)));

                assumptions[UpperBound].push_back(Z3_mk_ge(ctx, outputRateVar, avgOutputRate));
                assumptions[UpperBound].push_back(Z3_mk_eq(ctx, outputRateVar, maximum(outputRate)));
            }
        }
    }

    const auto E = identifyLengthEqualityAssertions(G);
    for (const auto e : make_iterator_range(edges(E))) {
        const auto A = VarList[source(e, G)];
        const auto B = VarList[target(e, G)];

        Z3_solver_assert(ctx, solver, Z3_mk_eq(ctx, A, B));
    }

    if (LLVM_UNLIKELY(Z3_solver_check(ctx, solver) == Z3_L_FALSE)) {
        report_fatal_error("Z3 failed to find a solution to the core dataflow graph");
    }

    Z3_solver_push(ctx, solver);
    const auto m = Z3_maxsat(ctx, solver, assumptions[Common]);
    if (LLVM_UNLIKELY(m < 1)) {
        Z3_solver_pop(ctx, solver, 1);
        Z3_solver_check(ctx, solver);
    }

    std::vector<Bound> current(PipelineOutput + 1);

    for (;;) {

        for (unsigned bound = LowerBound; bound <= UpperBound; ++bound) {

            Z3_solver_push(ctx, solver);
            const auto m = Z3_maxsat(ctx, solver, assumptions[bound]);
            if (LLVM_UNLIKELY(m < 1)) {
                Z3_solver_pop(ctx, solver, 1);
                Z3_solver_check(ctx, solver);
            }

            const auto model = Z3_solver_get_model(ctx, solver);
            Z3_model_inc_ref(ctx, model);
            for (auto kernel = firstKernel; kernel <= lastKernel; ++kernel) {

                Z3_ast const stridesPerSegmentVar = VarList[kernel];
                Z3_ast value;
                if (LLVM_UNLIKELY(Z3_model_eval(ctx, model, stridesPerSegmentVar, Z3_L_TRUE, &value) != Z3_L_TRUE)) {
                    report_fatal_error("Unexpected Z3 error when attempting to obtain value from model!");
                }

                __int64 num, denom;
                if (LLVM_UNLIKELY(Z3_get_numeral_rational_int64(ctx, value, &num, &denom) != Z3_L_TRUE)) {
                    report_fatal_error("Unexpected Z3 error when attempting to convert model value to number!");
                }
                assert (num > 0);
                current[kernel][bound] = Rational{num, denom};
            }
            Z3_model_dec_ref(ctx, model);

            Z3_solver_pop(ctx, solver, 1);
        }



        // If we find a solution but discover that min > max for any bound, we're going to have to
        // recompute the solution. This unfortunately does discard the learned clauses from the prior
        // lower/upper bound checks but appears that the amount of duplicate work is relatively small.

        // TODO: is there a way to have "divergent" solvers from the common assumption checks?

        bool done = true;
        for (auto kernel = firstKernel; kernel <= lastKernel; ++kernel) {
            const Bound & bound = current[kernel];
            if (LLVM_UNLIKELY(bound[LowerBound] > bound[UpperBound])) {
                const auto stridesPerSegmentVar = VarList[kernel];
                assumptions[LowerBound].push_back(Z3_mk_le(ctx, stridesPerSegmentVar, constant(bound[UpperBound])));
                assumptions[UpperBound].push_back(Z3_mk_ge(ctx, stridesPerSegmentVar, constant(bound[LowerBound])));
                done = false;
            }
        }

        if (LLVM_LIKELY(done)) {
            break;
        }

    }

    Z3_solver_dec_ref(ctx, solver);
    Z3_del_context(ctx);

    Z3_finalize_memory();

    const auto & b = current[firstKernel];
    auto g = gcd(b[LowerBound], b[UpperBound]);
    for (auto kernel = firstKernel + 1; kernel <= lastKernel; ++kernel) {
        const auto & b = current[kernel];
        g = gcd(g, b[LowerBound]);
        g = gcd(g, b[UpperBound]);
    }
    if (LLVM_UNLIKELY(g > Rational{1})) {
        for (auto kernel = firstKernel; kernel <= lastKernel; ++kernel) {
            auto & r = current[kernel];
            r[LowerBound] /= g;
            r[UpperBound] /= g;
        }
    }

    // Then write them out
    MinimumNumOfStrides.resize(PipelineOutput + 1);
    MaximumNumOfStrides.resize(PipelineOutput + 1);

    for (auto kernel = firstKernel; kernel <= lastKernel; ++kernel) {
        const auto & r = current[kernel];
        MinimumNumOfStrides[kernel] = r[LowerBound];
        MaximumNumOfStrides[kernel] = r[UpperBound];
    }

}

} // end of kernel namespace

#endif // DATAFLOW_ANALYSIS_HPP
