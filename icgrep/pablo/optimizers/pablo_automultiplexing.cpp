#include "pablo_automultiplexing.hpp"
#include <pablo/codegenstate.h>
#include <llvm/ADT/BitVector.h>
#include <stack>
#include <queue>
#include <unordered_set>
#include <boost/container/flat_set.hpp>
#include <unordered_map>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/circular_buffer.hpp>
#include <include/simd-lib/builtins.hpp>
#include <pablo/expression_map.hpp>
#include <pablo/printer_pablos.h>
#include <iostream>
#include <cudd.h>
#include <util.h>

using namespace llvm;
using namespace boost;
using namespace boost::container;
using namespace boost::numeric::ublas;

#define USE_WEIGHTED_SELECTION

namespace pablo {

typedef uint64_t timestamp_t;

static inline timestamp_t read_cycle_counter() {
#ifdef __GNUC__
timestamp_t ts;
#ifdef __x86_64__
  unsigned int eax, edx;
  asm volatile("rdtsc" : "=a" (eax), "=d" (edx));
  ts = ((timestamp_t) eax) | (((timestamp_t) edx) << 32);
#else
  asm volatile("rdtsc\n" : "=A" (ts));
#endif
  return(ts);
#endif
#ifdef _MSC_VER
  return __rdtsc();
#endif
}


bool AutoMultiplexing::optimize(const std::vector<Var *> & input, PabloBlock & entry) {
    bool modified = false;
    AutoMultiplexing am;
    const timestamp_t start = read_cycle_counter();
    am.initialize(input, entry);
    const timestamp_t end_initialize = read_cycle_counter();
    am.characterize(entry);
    const timestamp_t end_characterize = read_cycle_counter();
    am.shutdown();
    const timestamp_t end_shutdown = read_cycle_counter();
    am.createMultiplexSetGraph();
    std::random_device rd;
    const auto seed = rd(); // 573650087;
    RNG rng(seed);
    const bool multiplex = am.generateMultiplexSets(rng);
    const timestamp_t end_create_multiplex_graph = read_cycle_counter();
    timestamp_t end_mwis = end_create_multiplex_graph,
            end_subset_constraints = end_create_multiplex_graph,
            end_select_independent_sets = end_create_multiplex_graph,
            end_topological_sort = end_create_multiplex_graph;

    if (multiplex) {
        am.approxMaxWeightIndependentSet(rng);
        end_mwis = read_cycle_counter();
        am.applySubsetConstraints();
        end_subset_constraints = read_cycle_counter();
        am.multiplexSelectedIndependentSets();
        end_select_independent_sets = read_cycle_counter();
        am.topologicalSort(entry);
        end_topological_sort = read_cycle_counter();
        modified = true;
    }

//    std::cerr << "Initialize:              " << (end_initialize - start) << "   (seed=" << seed << ')' << std::endl;
//    std::cerr << "Characterize:            " << (end_characterize - end_initialize) << std::endl;
//    std::cerr << "Shutdown:                " << (end_shutdown - end_characterize) << std::endl;
//    std::cerr << "GenerateMultiplexSets:   " << (end_create_multiplex_graph - end_shutdown) << std::endl;
//    std::cerr << "MaxWeightIndependentSet: " << (end_mwis - end_create_multiplex_graph) << std::endl;
//    std::cerr << "ApplySubsetConstraints:  " << (end_subset_constraints - end_mwis) << std::endl;
//    std::cerr << "MultiplexSelectedSets:   " << (end_select_independent_sets - end_subset_constraints) << std::endl;
//    std::cerr << "TopologicalSort:         " << (end_topological_sort - end_select_independent_sets) << std::endl;

    return modified;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief initialize
 * @param vars the input vars for this program
 * @param entry the entry block
 *
 * Scan through the program to identify any advances and calls then initialize the BDD engine with
 * the proper variable ordering.
 ** ------------------------------------------------------------------------------------------------------------- */
void AutoMultiplexing::initialize(const std::vector<Var *> & vars, const PabloBlock & entry) {

    flat_map<const PabloAST *, unsigned> map;
    std::vector<const Statement *> complexStatements;
    std::stack<const Statement *> scope;

    // Scan through and collect all the advances, calls, scanthrus and matchstars ...
    unsigned n = 0, m = 0;
    for (const Statement * stmt = entry.front(); ; ) {
        while ( stmt ) {
            ++n;
            if (LLVM_UNLIKELY(isa<If>(stmt) || isa<While>(stmt))) {
                // Set the next statement to be the first statement of the inner scope and push the
                // next statement of the current statement into the scope stack.
                const PabloBlock & nested = isa<If>(stmt) ? cast<If>(stmt)->getBody() : cast<While>(stmt)->getBody();
                scope.push(stmt->getNextNode());
                stmt = nested.front();
                assert (stmt);
                continue;
            }

            assert ("Run the Simplifer pass prior to this!" && (stmt->getNumUses() != 0 || (isa<Assign>(stmt) ? !cast<Assign>(stmt)->superfluous() : false)));

            switch (stmt->getClassTypeId()) {
                case PabloAST::ClassTypeId::Advance:                    
                    mAdvance.push_back(const_cast<Advance*>(cast<Advance>(stmt)));
                    map.emplace(stmt, m++);
                case PabloAST::ClassTypeId::Call:
                case PabloAST::ClassTypeId::ScanThru:
                case PabloAST::ClassTypeId::MatchStar:
                    complexStatements.push_back(stmt);
                    break;
                default:
                    break;
            }
            stmt = stmt->getNextNode();
        }
        if (scope.empty()) {
            break;
        }
        stmt = scope.top();
        scope.pop();
    }

    // Create the transitive closure matrix of graph. From this we'll construct
    // two graphs restricted to the relationships between advances. The first will
    // be a path graph, which is used to bypass testing for mutual exclusivity of
    // advances that cannot be multiplexed. The second is a transitive reduction
    // of that graph, which forms the basis of our constraint graph when deciding
    // which advances ought to be multiplexed.

    matrix<bool> G(n, m); // Let G be a matrix with n rows of m (Advance) elements
    G.clear();
    for (unsigned i = 0; i != m; ++i) {
        G(i, i) = true;
    }

    n = m;
    m = 0;

    const Statement * stmt = entry.front();
    for (;;) {
        while ( stmt ) {

            unsigned u;
            if (isa<Advance>(stmt)) {
                assert (mAdvance[m] == stmt);
                u = m++;
            }
            else {
                u = n++;
                map.emplace(stmt, u);
            }

            for (unsigned i = 0; i != stmt->getNumOperands(); ++i) {
                const PabloAST * const op = stmt->getOperand(i);
                if (LLVM_LIKELY(isa<Statement>(op))) {
                    const unsigned v = map[op];
                    for (unsigned w = 0; w != m; ++w) {
                        G(u, w) |= G(v, w);
                    }
                }
            }
            if (LLVM_UNLIKELY(isa<If>(stmt) || isa<While>(stmt))) {
                // Set the next statement to be the first statement of the inner scope
                // and push the next statement of the current statement into the stack.
                const PabloBlock & nested = isa<If>(stmt) ? cast<If>(stmt)->getBody() : cast<While>(stmt)->getBody();
                scope.push(stmt->getNextNode());
                stmt = nested.front();
                assert (stmt);
                continue;
            }
            stmt = stmt->getNextNode();
        }
        if (scope.empty()) {
            break;
        }
        stmt = scope.top();
        scope.pop();
    }

    // Record our transitive closure in the path graph and remove any reflexive-loops
    mPathGraph = PathGraph(m);
    for (unsigned i = 0; i != m; ++i) {
        G(i, i) = false;
        for (unsigned j = 0; j != m; ++j) {
            if (G(i, j)) {
                add_edge(i, j, mPathGraph);
            }
        }        
    }

    // Now take the transitive reduction of G
    for (unsigned j = 0; j != m; ++j) {
        for (unsigned i = 0; i != m; ++i) {
            if (G(i, j)) {
                // Eliminate any unecessary arcs not covered by a longer path
                for (unsigned k = 0; k != m; ++k) {
                    G(i, k) = G(j, k) ? false : G(i, k);
                }
            }
        }
    }

    // And now transcribe it into our base constraint graph. Since G is a use-def
    // graph and we want our constraint graph to be a def-use graph, reverse the
    // edges as we're writing them to obtain the transposed graph.
    mConstraintGraph = ConstraintGraph(m);
    mSubsetGraph = SubsetGraph(m);
    for (unsigned j = 0; j != m; ++j) {
        for (unsigned i = 0; i != m; ++i) {
            if (G(i, j)) {
                add_edge(j, i, mConstraintGraph);
            }
        }
    }

    // Initialize the BDD engine ...
    mManager = Cudd_Init((complexStatements.size() + vars.size()), 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    Cudd_AutodynDisable(mManager);

    // Map the predefined 0/1 entries
    mCharacterizationMap.emplace(entry.createZeroes(), Zero());
    mCharacterizationMap.emplace(entry.createOnes(), One());

    // Order the variables so the input Vars are pushed to the end; they ought to
    // be the most complex to resolve.
    unsigned i = 0;
    for (const Statement * stmt : complexStatements) {
        mCharacterizationMap.emplace(stmt, Cudd_bddIthVar(mManager, i++));
    }
    for (const Var * var : vars) {
        mCharacterizationMap.emplace(var, Cudd_bddIthVar(mManager, i++));
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief characterize
 * @param vars the input vars for this program
 *
 * Scan through the program and iteratively compute the BDDs for each statement.
 ** ------------------------------------------------------------------------------------------------------------- */
void AutoMultiplexing::characterize(PabloBlock & entry) {

    std::vector<std::pair<DdNode *, DdNode *>> advances; // the input BDD and the BDD variable of the i-th Advance
    std::stack<Statement *> scope;
    std::vector<bool> unconstrained(mAdvance.size());
    advances.reserve(mAdvance.size());

    // TODO: What if we did some minterm analysis in here? We'd need to be careful to keep the Advances properly indexed.

    unsigned comparisons = 0;
    unsigned avoided = 0;

    // Scan through and collect all the advances, calls, scanthrus and matchstars ...
    for (Statement * stmt = entry.front(); ; ) {
        while ( stmt ) {

            if (LLVM_UNLIKELY(isa<If>(stmt) || isa<While>(stmt))) {
                // Set the next statement to be the first statement of the inner scope and push the
                // next statement of the current statement into the scope stack.
                const PabloBlock & nested = isa<If>(stmt) ? cast<If>(stmt)->getBody() : cast<While>(stmt)->getBody();
                scope.push(stmt->getNextNode());
                stmt = nested.front();
                continue;
            }

            DdNode * bdd = nullptr;
            // Map our operands to the computed BDDs
            std::array<DdNode *, 3> input;
            for (unsigned i = 0; i != stmt->getNumOperands(); ++i) {
                PabloAST * const op = stmt->getOperand(i);
                if (LLVM_UNLIKELY(isa<Integer>(op) || isa<String>(op))) {
                    continue;
                }
                auto f = mCharacterizationMap.find(op);
                if (LLVM_UNLIKELY(f == mCharacterizationMap.end())) {
                    throw std::runtime_error("Uncharacterized operand " + std::to_string(i) + " of " + stmt->getName()->to_string());
                }
                input[i] = f->second;
            }

            bool testContradiction = true;
            bool updateVariable = true;

            switch (stmt->getClassTypeId()) {
                case PabloAST::ClassTypeId::Assign:
                    bdd = input[0];
                    break;
                case PabloAST::ClassTypeId::And:
                    bdd = And(input[0], input[1]);
                    break;
                case PabloAST::ClassTypeId::Next:
                    // The next instruction is almost identical to an OR; however, a Next cannot be
                    // an operand of a Statement. Instead it updates the Initial operand's value.
                    testContradiction = false;
                case PabloAST::ClassTypeId::Or:            
                    bdd = Or(input[0], input[1]);
                    break;
                case PabloAST::ClassTypeId::Xor:
                    bdd = Xor(input[0], input[1]);
                    break;
                case PabloAST::ClassTypeId::Not:
                    bdd = Not(input[0]);
                    break;
                case PabloAST::ClassTypeId::Sel:
                    bdd = Ite(input[0], input[1], input[2]);
                    break;
                case PabloAST::ClassTypeId::ScanThru:
                    // It may be possible use "Not(input[1])" for ScanThrus if we rule out the possibility
                    // of a contradition being erroneously calculated later. An example of this
                    // would be ¬(ScanThru(c,m) ∨ m)
                case PabloAST::ClassTypeId::MatchStar:
                    if (LLVM_UNLIKELY(isZero(input[0]) || isZero(input[1]))) {
                        bdd = Zero();
                        break;
                    }
                case PabloAST::ClassTypeId::Call:
                    testContradiction = false;
                    updateVariable = false;
                    break;
                case PabloAST::ClassTypeId::Advance:
                    assert (stmt == mAdvance[advances.size()]);
                    if (LLVM_UNLIKELY(isZero(input[0]))) {
                        bdd = input[0];
                        advances.emplace_back(bdd, bdd);
                    }
                    else {

                        // When we built the path graph, we constructed it in the same order; hense the row/column of
                        // the path graph is equivalent to the index.

                        assert (mCharacterizationMap.count(stmt));
                        DdNode * const Ik = input[0];
                        DdNode * Ck = mCharacterizationMap[stmt];
                        DdNode * const Nk = Not(Ck);

                        const unsigned k = advances.size();

                        std::fill(unconstrained.begin(), unconstrained.end(), false);

                        // Instead of processing these sequentially, look at the existing subset relationships incase we can infer
                        // more things sooner?
                        for (unsigned i = 0; i != k; ++i) {

                            Advance * adv = mAdvance[i];

                            // Only test pairs if they are in the same scope or one has a user in the a scope that is reachable by
                            // the other?

                            bool constrained = !unconstrained[i];

                            // If these advances are "shifting" their values by the same amount and aren't transitively dependant ...
                            if (constrained && (stmt->getOperand(1) == adv->getOperand(1)) && (notTransitivelyDependant(k, i))) {
                                ++comparisons;
                                DdNode * Ii = std::get<0>(advances[i]);
                                DdNode * Ni = std::get<1>(advances[i]);
                                // TODO: test both Cudd_And and Cudd_bddIntersect to see which is faster
                                DdNode * const IiIk = Intersect(Ik, Ii);
                                // Is there any satisfying truth assignment? If not, these streams are mutually exclusive.
                                if (noSatisfyingAssignment(IiIk)) {
                                    assert (mCharacterizationMap.count(adv));
                                    DdNode *& Ci = mCharacterizationMap[adv];
                                    // Mark the i-th and k-th Advances as being mutually exclusive
                                    Ck = And(Ck, Ni);
                                    Ci = And(Ci, Nk);
                                    // If Ai ∩ Ak = ∅ and Aj ⊂ Ai, Aj ∩ Ak = ∅.
                                    graph_traits<SubsetGraph>::in_edge_iterator ei, ei_end;
                                    for (std::tie(ei, ei_end) = in_edges(i, mSubsetGraph); ei != ei_end; ++ei) {
                                        const auto j = source(*ei, mSubsetGraph);
                                        if (j > i && notTransitivelyDependant(k, j)) {
                                            Advance * adv = mAdvance[j];
                                            assert (mCharacterizationMap.count(adv));
                                            DdNode *& Cj = mCharacterizationMap[adv];
                                            DdNode * Nj = std::get<1>(advances[j]);
                                            // Mark the i-th and j-th Advances as being mutually exclusive
                                            Ck = And(Ck, Nj);
                                            Cj = And(Cj, Nk);
                                            unconstrained[j] = true;
                                            ++avoided;
                                        }
                                    }
                                    constrained = false;
                                }
                                else if (Ik == IiIk) {
                                    // If Ik = Ii ∩ Ik then Ik ⊂ Ii. Record this in the subset graph with the arc (k,i).
                                    // These edges will be moved into the multiplex set graph if Ai and Ak happen to be
                                    // in the same mutually exclusive set.
                                    add_edge(k, i, mSubsetGraph);
                                    // If Ak ⊂ Ai and Ai ⊂ Aj, Ak ⊂ Aj.
                                    graph_traits<SubsetGraph>::out_edge_iterator ei, ei_end;
                                    for (std::tie(ei, ei_end) = out_edges(i, mSubsetGraph); ei != ei_end; ++ei) {
                                        const auto j = target(*ei, mSubsetGraph);
                                        if (j > i) {
                                            add_edge(k, j, mSubsetGraph);
                                            unconstrained[j] = true;
                                            ++avoided;
                                        }
                                    }
                                    constrained = false;
                                }
                                else if (Ii == IiIk) {
                                    // If Ii = Ii ∩ Ik then Ii ⊂ Ik. Record this in the subset graph with the arc (i,k).
                                    add_edge(i, k, mSubsetGraph);
                                    // If Ai ⊂ Ak and Aj ⊂ Ai, Aj ⊂ Ak.
                                    graph_traits<SubsetGraph>::in_edge_iterator ei, ei_end;
                                    for (std::tie(ei, ei_end) = in_edges(i, mSubsetGraph); ei != ei_end; ++ei) {
                                        const auto j = source(*ei, mSubsetGraph);
                                        if (j > i) {
                                            add_edge(j, k, mSubsetGraph);
                                            unconstrained[j] = true;
                                            ++avoided;
                                        }
                                    }
                                    constrained = false;
                                }
                                Cudd_RecursiveDeref(mManager, IiIk);
                            }

                            // Even if these Advances are mutually exclusive, they must be in the same scope or they cannot be safely multiplexed.
                            if (constrained || (stmt->getParent() != adv->getParent())) {
                                // We want the constraint graph to be acyclic; since the dependencies are already in topological order,
                                // adding an arc from a lesser to greater numbered vertex won't induce a cycle.
                                add_edge(i, k, mConstraintGraph);
                            }

                        }
                        // Append this advance to the list of known advances. Both its input BDD and the Advance variable are stored in
                        // the list to eliminate the need for searching for it.
                        advances.emplace_back(Ik, Nk);
                        testContradiction = false;
                        bdd = Ck;
                    }
                    break;
                default:
                    throw std::runtime_error("Unexpected statement type " + stmt->getName()->to_string());
            }
            assert ("Failed to generate a BDD." && (bdd || !(testContradiction || updateVariable)));
            if (LLVM_LIKELY(updateVariable)) {
                mCharacterizationMap[stmt] = bdd;
            }
            if (LLVM_UNLIKELY(testContradiction && noSatisfyingAssignment(bdd))) {                
                if (!isa<Assign>(stmt)) {
                    Cudd_RecursiveDeref(mManager, bdd);
                    stmt = stmt->replaceWith(entry.createZeroes());
                    continue;
                }
            }
            stmt = stmt->getNextNode();
        }

        if (scope.empty()) {
            break;
        }
        stmt = scope.top();
        scope.pop();
    }

//    std::cerr << "comparisons: " << comparisons << ", avoided: " << avoided << std::endl;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief notTransitivelyDependant
 ** ------------------------------------------------------------------------------------------------------------- */
inline bool AutoMultiplexing::notTransitivelyDependant(const PathGraph::vertex_descriptor i, const PathGraph::vertex_descriptor j) const {
    return (mPathGraph.get_edge(i, j) == 0);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief CUDD wrappers
 ** ------------------------------------------------------------------------------------------------------------- */

inline DdNode * AutoMultiplexing::Zero() const {
    return Cudd_ReadLogicZero(mManager);
}

inline DdNode * AutoMultiplexing::One() const {
    return Cudd_ReadOne(mManager);
}

inline bool AutoMultiplexing::isZero(DdNode * const x) const {
    return x == Zero();
}

inline DdNode * AutoMultiplexing::And(DdNode * const x, DdNode * const y) {
    DdNode * r = Cudd_bddAnd(mManager, x, y);
    Cudd_Ref(r);
    return r;
}

inline DdNode * AutoMultiplexing::Intersect(DdNode * const x, DdNode * const y) {
    DdNode * r = Cudd_bddIntersect(mManager, x, y); Cudd_Ref(r);
    return r;
}

inline DdNode * AutoMultiplexing::Or(DdNode * const x, DdNode * const y) {
    DdNode * r = Cudd_bddOr(mManager, x, y);
    Cudd_Ref(r);
    return r;
}

inline DdNode * AutoMultiplexing::Xor(DdNode * const x, DdNode * const y) {
    DdNode * r = Cudd_bddXor(mManager, x, y);
    Cudd_Ref(r);
    return r;
}

inline DdNode * AutoMultiplexing::Not(DdNode * const x) const {
    return Cudd_Not(x);
}

inline DdNode * AutoMultiplexing::Ite(DdNode * const x, DdNode * const y, DdNode * const z) {
    DdNode * r = Cudd_bddIte(mManager, x, y, z);
    Cudd_Ref(r);
    return r;
}

inline bool AutoMultiplexing::noSatisfyingAssignment(DdNode * const x) {
    return Cudd_bddLeq(mManager, x, Zero());
}

inline void AutoMultiplexing::shutdown() {
    Cudd_Quit(mManager);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief createMultiplexSetGraph
 ** ------------------------------------------------------------------------------------------------------------- */
void AutoMultiplexing::createMultiplexSetGraph() {
    mMultiplexSetGraph = MultiplexSetGraph(num_vertices(mConstraintGraph));
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief generateMultiplexSets
 * @param RNG random number generator
 ** ------------------------------------------------------------------------------------------------------------- */
bool AutoMultiplexing::generateMultiplexSets(RNG & rng) {

    using Vertex = ConstraintGraph::vertex_descriptor;
    using VertexIterator = graph_traits<ConstraintGraph>::vertex_iterator;
    using EdgeIterator = graph_traits<ConstraintGraph>::out_edge_iterator;
    using DegreeType = graph_traits<ConstraintGraph>::degree_size_type;

    IndependentSet S, N;
    auto remainingVerticies = num_vertices(mConstraintGraph);
    std::vector<DegreeType> D(remainingVerticies);
    S.reserve(15);
    N.reserve(15);

    // Push all source nodes into the new independent set N
    VertexIterator vi, vi_end;
    for (std::tie(vi, vi_end) = vertices(mConstraintGraph); vi != vi_end; ++vi) {
        const auto d = in_degree(*vi, mConstraintGraph);
        D[*vi] = d;
        if (d == 0) {
            N.push_back(*vi);
        }
    }

    while ( remainingVerticies >= 3 ) {


        // If we have at least 3 vertices in our two sets, try adding them to our
        // multiplexing set.
        if ((N.size() + S.size()) >= 3) {
            addMultiplexSet(N, S);
        }        

        // Move all of our "newly" uncovered vertices into the "known" set. By always
        // choosing at least one element from N, this will prevent us from adding the
        // same multiplexing set again.
        S.insert(S.end(), N.begin(), N.end()); N.clear();

        do {
            // Randomly choose a vertex in S and discard it.
            assert (!S.empty());
            const auto i = S.begin() + RNGDistribution(0, S.size() - 1)(rng);
            const Vertex u = *i;
            S.erase(i);
            --remainingVerticies;
            EdgeIterator ei, ei_end;
            for (std::tie(ei, ei_end) = out_edges(u, mConstraintGraph); ei != ei_end; ++ei) {
                const Vertex v = target(*ei, mConstraintGraph);
                if ((--D[v]) == 0) {
                    N.push_back(v);
                }
            }
        }
        while (N.empty() && remainingVerticies >= 3);
    }

    return num_vertices(mMultiplexSetGraph) > num_vertices(mConstraintGraph);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief is_power_of_2
 * @param n an integer
 ** ------------------------------------------------------------------------------------------------------------- */
static inline bool is_power_of_2(const size_t n) {
    return ((n & (n - 1)) == 0) ;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief addMultiplexSet
 * @param N an independent set
 * @param S an independent set
 ** ------------------------------------------------------------------------------------------------------------- */
inline void AutoMultiplexing::addMultiplexSet(const IndependentSet & N, const IndependentSet & M) {

    // At this stage, the multiplex set graph is a directed bipartite graph that is used to show relationships
    // between the "set vertex" and its members. We obtain these from "generateMultiplexSets".

    // Note: this computes (∑i=1 to |N| C(|N|,i)) * [1 + (∑i=1 to |M| C(|M|,i))] = (2^|N| - 1) * (2^|M|) combinations.

    ChosenSet S;
    for (int i = 0; i < N.size(); ++i) {
        S.insert(N[i]);
        addMultiplexSet(N, i + 1, M, 0, S);
        S.erase(S.find(N[i]));
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief addMultiplexSet
 * @param N an independent set
 * @param S an independent set
 ** ------------------------------------------------------------------------------------------------------------- */
void AutoMultiplexing::addMultiplexSet(const IndependentSet & N, const int i,
                                       const IndependentSet & M, const int j,
                                       ChosenSet & S) {

    // TODO: Instead of building a graph, construct a trie of all members.

    // Add S to the multiplex set if |S| >= 3 and not equal to 2^n for any n.
    if (S.size() >= 3 && !is_power_of_2(S.size())) {
        const auto v = add_vertex(mMultiplexSetGraph);
        for (auto u : S) {
            add_edge(v, u, mMultiplexSetGraph);
        }
    }

    for (int ii = i; ii < N.size(); ++ii) {
        S.insert(N[ii]);
        addMultiplexSet(N, ii + 1, M, j, S);
        S.erase(S.find(N[ii]));
    }

    for (int jj = j; jj < M.size(); ++jj) {
        S.insert(M[jj]);
        addMultiplexSet(N, i + 1, M, jj + 1, S);
        S.erase(S.find(M[jj]));
    }

}

static inline ssize_t sqr(const ssize_t n) {
    return n * n;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief approxMaxWeightIndependentSet
 * @param RNG random number generator
 ** ------------------------------------------------------------------------------------------------------------- */
void AutoMultiplexing::approxMaxWeightIndependentSet(RNG & rng) {

    // Compute our independent set graph from our multiplex set graph; effectively it's the graph resulting
    // from contracting one advance node edge with an adjacent vertex

    const unsigned m = num_vertices(mConstraintGraph);
    const unsigned n = num_vertices(mMultiplexSetGraph) - m;

    const auto start = read_cycle_counter();

    IndependentSetGraph G(n);

    MultiplexSetGraph::vertex_descriptor i = m;
    graph_traits<IndependentSetGraph>::vertex_iterator vi, vi_end;
    for (std::tie(vi, vi_end) = vertices(G); vi != vi_end; ++vi, ++i) {
        G[*vi] = std::make_pair(out_degree(i, mMultiplexSetGraph), 0);
    }

    // Let M be mMultiplexSetGraph. Each vertex in G corresponds to a set vertex in M.
    // If an advance vertex in M (i.e., one of the first m vertices of M) is adjacent
    // to two or more set vertices, add an edge between the corresponding vertices in G.
    // I.e., make all vertices in G adjacent if their corresponding sets intersect.

    for (i = 0; i != m; ++i) {
        if (in_degree(i, mMultiplexSetGraph) > 1) {
            graph_traits<MultiplexSetGraph>::in_edge_iterator ei_begin, ei_end;
            std::tie(ei_begin, ei_end) = in_edges(i, mMultiplexSetGraph);
            for (auto ei = ei_begin; ei != ei_end; ++ei) {
                for (auto ej = ei_begin; ej != ei; ++ej) {
                    // Note: ei and ej are incoming edges. The source is the set vertex,
                    add_edge(source(*ei, mMultiplexSetGraph) - m, source(*ej, mMultiplexSetGraph) - m, G);
                }
            }
        }
    }

    boost::container::flat_set<IndependentSetGraph::vertex_descriptor> indices;
    indices.reserve(n);
    #ifdef USE_WEIGHTED_SELECTION
    for (std::tie(vi, vi_end) = vertices(G); vi != vi_end; ++vi, ++i) {
        int neighbourhoodWeight = 0;
        int neighbours = 1;
        graph_traits<IndependentSetGraph>::adjacency_iterator ai, ai_end;
        for (std::tie(ai, ai_end) = adjacent_vertices(*vi, G); ai != ai_end; ++ai) {
            ++neighbours;
            neighbourhoodWeight += std::get<0>(G[*ai]);
        }
        std::get<1>(G[*vi]) = (std::get<0>(G[*vi]) * neighbours) - neighbourhoodWeight;
        indices.insert(*vi);
    }
    #endif

    const auto built_graph = read_cycle_counter();

    // std::cerr << "G=(" << num_vertices(G) << ',' << num_edges(G) << ") built in " << (built_graph - start) << std::endl;

    // Using WMIN from "A note on greedy algorithms for the maximum weighted independent set problem"
    // (Note: look into minimum independent dominating set algorithms. It fits better with the ceil log2 + subset cost model.)

    while (!indices.empty()) {

        // Select a vertex vi whose weight as greater than the average weight of its neighbours ...
        #ifdef USE_WEIGHTED_SELECTION
        int totalWeight = 0;
        for (auto vi = indices.begin(); vi != indices.end(); ++vi) {
            totalWeight += sqr(std::get<1>(G[*vi]));
        }

        IndependentSetGraph::vertex_descriptor v = 0;
        ssize_t remainingWeight = RNGDistribution(0, totalWeight - 1)(rng);
        for (auto vi = indices.begin(); vi != indices.end(); ++vi) {
            remainingWeight -= sqr(std::max<int>(std::get<1>(G[*vi]), 0));
            if (remainingWeight < 0) {
                v = *vi;
                break;
            }
        }
        assert (remainingWeight < 0);

        #else
        auto i = RNGDistribution(1, indices.size())(rng);
        for (std::tie(vi, vi_end) = vertices(G); --i != 0; ++vi);
        assert (vi != vi_end);
        #endif        

        // Note: by clearing the adjacent set vertices from the multiplex set graph, this will effectively
        // choose the set refered to by vi as one of our chosen independent sets.
        graph_traits<IndependentSetGraph>::adjacency_iterator ai, ai_end, aj, aj_end;
        for (std::tie(ai, ai_end) = adjacent_vertices(v, G); ai != ai_end; ) {
            const auto u = *ai++;
            assert (indices.count(u));
            indices.erase(indices.find(u));
            clear_out_edges(u + m, mMultiplexSetGraph);
            #ifdef USE_WEIGHTED_SELECTION
            const auto Wj = std::get<0>(G[u]);
            for (std::tie(aj, aj_end) = adjacent_vertices(u, G); aj != aj_end; ) {

                // Let Vi be vertex i, Wi = weight of Vi, Di = degree of Vi, Ni = sum of the weights of vertices
                // adjacent to Vi, and Ai be the weight difference of Vi w.r.t. its neighbours. Initially,

                //     Ai = (Wi * (Di + 1)) - Ni

                // Suppose Vj is removed from G and is adjacent to Vi. Then,

                //    Ai' = (Wi * (Di + 1 - 1)) - (Ni - Wj) = (Ai - Wi + Wj)

                const auto w = *aj++;
                const auto Wi = std::get<0>(G[w]);
                auto & Ai = std::get<1>(G[w]);
                Ai = Ai - Wi + Wj;
                remove_edge(u, w, G);
            }
            #else
            clear_vertex(u, G);
            #endif
            remove_edge(v, u, G);
        }
        indices.erase(indices.find(v));
    }

//    for (unsigned i = m; i != (m + n); ++i) {
//        if (out_degree(i, mMultiplexSetGraph) > 0) {
//            std::cerr << out_degree(i, mMultiplexSetGraph) << std::endl;
//        }
//    }

    #ifndef NDEBUG
    for (unsigned i = 0; i != m; ++i) {
        assert (in_degree(i, mMultiplexSetGraph) <= 1);
    }
    for (unsigned i = m; i != (m + n); ++i) {
        assert (out_degree(i, mMultiplexSetGraph) == 0 || out_degree(i, mMultiplexSetGraph) >= 3);
    }
    #endif
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief choose
 ** ------------------------------------------------------------------------------------------------------------- */

inline Statement * choose(PabloAST * x, PabloAST * y, Statement * z) {
    return isa<Statement>(x) ? cast<Statement>(x) : (isa<Statement>(y) ? cast<Statement>(y) : z);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief applySubsetConstraints
 ** ------------------------------------------------------------------------------------------------------------- */
void AutoMultiplexing::applySubsetConstraints() {

    // When entering thus function, the multiplex set graph M is a bipartite DAG with edges denoting the set
    // relationships of our independent sets.
    const unsigned m = num_vertices(mConstraintGraph);
    const unsigned n = num_vertices(mMultiplexSetGraph);

    // Add in any edges from our subset constraints to M that are within the same set.
    bool hasSubsetConstraint = false;

    graph_traits<SubsetGraph>::edge_iterator ei, ei_end;
    for (std::tie(ei, ei_end) = edges(mSubsetGraph); ei != ei_end; ++ei) {
        const auto u = source(*ei, mSubsetGraph);
        const auto v = target(*ei, mSubsetGraph);
        graph_traits<MultiplexSetGraph>::in_edge_iterator ej, ej_end;
        // If both the source and target of ei are adjacent to the same vertex, that vertex must be the
        // "set vertex". Add the edge between the vertices.
        for (std::tie(ej, ej_end) = in_edges(u, mMultiplexSetGraph); ej != ej_end; ++ej) {
            auto w = target(*ej, mMultiplexSetGraph);
            // Only check (v, w) if w is a "set vertex".
            if (w >= (n - m) && edge(v, w, mMultiplexSetGraph).second) {
                add_edge(u, v, mMultiplexSetGraph);
                hasSubsetConstraint = true;
            }
        }
    }

    if (LLVM_UNLIKELY(hasSubsetConstraint)) {
        // At this point, M is still a DAG but no longer bipartite. We're going to scan through the set vertices
        // in M, and use a DFS to scan through M and eliminate any subset relationships in the AST.
        // That way, "multiplexSelectedIndependentSets" only needs to consider muxing and demuxing of the streams.

        std::vector<MultiplexSetGraph::vertex_descriptor> V;
        std::stack<MultiplexSetGraph::vertex_descriptor> S;
        std::queue<PabloAST *> Q;
        BitVector D(n - m, 0);

        for (auto i = m; i != n; ++i) {
            graph_traits<MultiplexSetGraph>::out_edge_iterator ei, ei_end;
            // For each member of a "set vertex" ...
            std::tie(ei, ei_end) = out_edges(i, mMultiplexSetGraph);
            for (; ei != ei_end; ++ei) {
                const auto s = source(*ei, mMultiplexSetGraph);
                if (out_degree(s, mMultiplexSetGraph) != 0) {
                    // First scan through the subgraph of vertices in M dominated by s and build up the set T,
                    // consisting of all sinks w.r.t. vertex s.
                    auto u = s;
                    for (;;) {
                        graph_traits<MultiplexSetGraph>::out_edge_iterator ej, ej_end;
                        for (std::tie(ej, ej_end) = out_edges(u, mMultiplexSetGraph); ej != ej_end; ++ej) {
                            auto v = target(*ej, mMultiplexSetGraph);
                            if (D.test(v)) {
                                continue;
                            }
                            D.set(v);
                            if (out_degree(v, mMultiplexSetGraph) == 0) {
                                V.push_back(v);
                            }
                            else {
                                S.push(v);
                            }
                        }
                        if (S.empty()) {
                            break;
                        }
                        u = S.top();
                        S.pop();
                    }
                    D.clear();
                    // Now in order for these advances to be mutually exclusive, the input to A_s must be masked
                    // with the complement of each advance indicated by V.

                    Advance * adv = mAdvance[s];
                    PabloBlock * pb = adv->getParent();

                    for (auto i : V) {
                        Q.push(mAdvance[i]->getOperand(0));
                    }                    
                    while (Q.size() > 1) {
                        PabloAST * a1 = Q.front(); Q.pop();
                        PabloAST * a2 = Q.front(); Q.pop();
                        pb->setInsertPoint(cast<Statement>(a2));
                        Q.push(pb->createOr(a1, a2));
                    }
                    assert (Q.size() == 1);

                    PabloAST * const mask = pb->createNot(Q.front()); Q.pop();
                    adv->setOperand(0, pb->createAnd(adv->getOperand(0), mask, "subset_in"));

                    // Similar to the above, we're going to OR together the result of each advance,
                    // including s. This will restore the advanced variable back to its original state.

                    // Gather the original users to this advance. We'll be manipulating it shortly.
                    Statement::Users U(adv->users());

                    // Add s to V and sort the list; it'll be closer to being in topological order.
                    V.push_back(s);
                    std::sort(V.begin(), V.end());
                    for (auto i : V) {
                        Q.push(mAdvance[i]);
                    }
                    while (Q.size() > 1) {
                        PabloAST * a1 = Q.front(); Q.pop();
                        PabloAST * a2 = Q.front(); Q.pop();
                        pb->setInsertPoint(choose(a2, a1, adv));
                        Q.push(pb->createOr(a1, a2));
                    }
                    assert (Q.size() == 1);

                    PabloAST * const input = Q.front(); Q.pop();
                    for (PabloAST * use : U) {
                        if (LLVM_LIKELY(isa<Statement>(use))) {
                            cast<Statement>(use)->replaceUsesOfWith(adv, input);
                        }
                    }

                    pb->setInsertPoint(pb->back());

                    V.clear();

                }
            }
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief log2_plus_one
 ** ------------------------------------------------------------------------------------------------------------- */
static inline size_t log2_plus_one(const size_t n) {
    return std::log2<size_t>(n) + 1; // use a faster builtin function for this?
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief multiplexSelectedIndependentSets
 ** ------------------------------------------------------------------------------------------------------------- */
void AutoMultiplexing::multiplexSelectedIndependentSets() const {

    const unsigned f = num_vertices(mConstraintGraph);
    const unsigned l = num_vertices(mMultiplexSetGraph);

    // Preallocate the structures based on the size of the largest multiplex set
    size_t max_n = 3;
    for (unsigned s = f; s != l; ++s) {
        max_n = std::max<unsigned>(max_n, out_degree(s, mMultiplexSetGraph));
    }
    const size_t max_m = log2_plus_one(max_n);

    std::vector<MultiplexSetGraph::vertex_descriptor> I(max_n);
    std::vector<Advance *> V(max_n);
    std::vector<PabloAST *> muxed(max_m);
    circular_buffer<PabloAST *> Q(max_n);

    // When entering thus function, the multiplex set graph M is a DAG with edges denoting the set
    // relationships of our independent sets.

    for (unsigned s = f; s != l; ++s) {
        const size_t n = out_degree(s, mMultiplexSetGraph);
        if (n) {

            const size_t m = log2_plus_one(n);

            graph_traits<MultiplexSetGraph>::out_edge_iterator ei, ei_end;
            std::tie(ei, ei_end) = out_edges(s, mMultiplexSetGraph);
            for (unsigned i = 0; i != n; ++ei, ++i) {
                I[i] = target(*ei, mMultiplexSetGraph);
                assert (I[i] < mAdvance.size());
            }
            std::sort(I.begin(), I.begin() + n);

            for (unsigned i = 0; i != n; ++i) {
                V[i] = mAdvance[I[i]];
            }

            PabloBlock * const pb = V[0]->getParent();
            assert (pb);

            // Sanity test to make sure every advance is in the same scope.
            #ifndef NDEBUG
            for (unsigned i = 1; i != n; ++i) {
                assert (I[i - 1] < I[i]);
                assert (V[i - 1] != V[i]);
                assert (V[i]->getParent() == pb);
            }
            #endif

            /// Perform n-to-m Multiplexing
            for (size_t j = 0; j != m; ++j) {

                assert (Q.empty());

                std::ostringstream prefix;

                prefix << "mux" << n << "to" << m;
                for (size_t i = 1; i <= n; ++i) {
                    if ((i & (static_cast<size_t>(1) << j)) != 0) {
                        assert (!Q.full());
                        PabloAST * tmp = V[i - 1]->getOperand(0); assert (tmp);
                        prefix << '_' << V[i - 1]->getName()->to_string();
                        Q.push_back(tmp);
                    }
                }

                assert (Q.size() >= 1);

                Advance * const adv = V[j];
                // TODO: figure out a way to determine whether we're creating a duplicate value below.
                // The expression map findOrCall ought to work conceptually but the functors method
                // doesn't really work anymore with the current API.
                while (Q.size() > 1) {
                    PabloAST * a1 = Q.front(); Q.pop_front(); assert (a1);
                    PabloAST * a2 = Q.front(); Q.pop_front(); assert (a2);
                    assert (!Q.full());
                    pb->setInsertPoint(choose(a2, a1, adv));
                    Q.push_back(pb->createOr(a1, a2));
                }
                assert (Q.size() == 1);

                PabloAST * mux = Q.front(); Q.pop_front(); assert (mux);
                muxed[j] = pb->createAdvance(mux, adv->getOperand(1), prefix.str());
            }


            /// Perform m-to-n Demultiplexing
            // Now construct the demuxed values and replaces all the users of the original advances with them.
            for (size_t i = 1; i <= n; ++i) {

                Advance * const adv = V[i - 1];

                pb->setInsertPoint(adv);

                assert (Q.empty());
                for (size_t j = 0; j != m; ++j) {
                    if ((i & (static_cast<size_t>(1) << j)) == 0) {
                        Q.push_back(muxed[j]);
                    }
                }

                assert (Q.size() <= m);
                PabloAST * neg = nullptr;
                if (LLVM_LIKELY(Q.size() > 0)) {
                    while (Q.size() > 1) {
                        PabloAST * a1 = Q.front(); Q.pop_front(); assert (a1);
                        PabloAST * a2 = Q.front(); Q.pop_front(); assert (a2);
                        assert (!Q.full());
                        Q.push_back(pb->createOr(a1, a2));
                    }
                    assert (Q.size() == 1);
                    neg = pb->createNot(Q.front()); Q.pop_front(); assert (neg);
                }

                assert (Q.empty());
                for (unsigned j = 0; j != m; ++j) {
                    if ((i & (static_cast<unsigned>(1) << j)) != 0) {
                        assert (!Q.full());
                        Q.push_back(muxed[j]);
                    }
                }

                assert (Q.size() <= m);
                assert (Q.size() >= 1);

                while (Q.size() > 1) {
                    PabloAST * a1 = Q.front(); Q.pop_front(); assert (a1);
                    PabloAST * a2 = Q.front(); Q.pop_front(); assert (a2);
                    assert (!Q.full());
                    Q.push_back(pb->createAnd(a1, a2));
                }

                assert (Q.size() == 1);

                PabloAST * demux = Q.front(); Q.pop_front(); assert (demux);
                if (LLVM_LIKELY(neg != nullptr)) {
                    demux = pb->createAnd(demux, neg);
                }
                V[i - 1]->replaceWith(demux, true, true);
            }
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief topologicalSort
 *
 * After transforming the IR, we need to run this in order to always have a valid program. Each multiplex set
 * contains vertices corresponding to an Advance in the IR. While we know each Advance within a set is independent
 * w.r.t. the transitive closure of their dependencies in the IR, the position of each Advance's dependencies and
 * users within the IR isn't taken into consideration. Thus while there must be a valid ordering for the program,
 * it's not necessarily the original ordering.
 ** ------------------------------------------------------------------------------------------------------------- */
void AutoMultiplexing::topologicalSort(PabloBlock & entry) const {
    // Note: not a real topological sort. I expect the original order to be very close to the resulting one.
    std::unordered_set<const PabloAST *> encountered;
    std::stack<Statement *> scope;
    for (Statement * stmt = entry.front(); ; ) { restart:
        while ( stmt ) {
            for (unsigned i = 0; i != stmt->getNumOperands(); ++i) {
                PabloAST * const op = stmt->getOperand(i);
                if (LLVM_LIKELY(isa<Statement>(op))) {
                    if (LLVM_UNLIKELY(encountered.count(op) == 0)) {
                        if (LLVM_UNLIKELY(isa<While>(stmt) && isa<Next>(op))) {
                            if (encountered.count(cast<Next>(op)->getInitial()) != 0) {
                                continue;
                            }
                        }
                        Statement * const next = stmt->getNextNode();
                        stmt->insertAfter(cast<Statement>(op));
                        stmt = next;
                        goto restart;
                    }
                }
            }
            if (LLVM_UNLIKELY(isa<If>(stmt) || isa<While>(stmt))) {
                // Set the next statement to be the first statement of the inner scope and push the
                // next statement of the current statement into the scope stack.
                const PabloBlock & nested = isa<If>(stmt) ? cast<If>(stmt)->getBody() : cast<While>(stmt)->getBody();
                scope.push(stmt->getNextNode());
                stmt = nested.front();
                continue;
            }
            encountered.insert(stmt);
            stmt = stmt->getNextNode();
        }
        if (scope.empty()) {
            break;
        }
        stmt = scope.top();
        scope.pop();
    }
}

} // end of namespace pablo

