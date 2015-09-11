#include "booleanreassociationpass.h"
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/strong_components.hpp>
#include <pablo/optimizers/pablo_simplifier.hpp>
#include <pablo/analysis/pabloverifier.hpp>
#include <algorithm>
#include <queue>
#include <set>
#include <iostream>
#include <pablo/printer_pablos.h>
#include "graph-facade.hpp"

using namespace boost;
using namespace boost::container;

namespace pablo {

using Graph = BooleanReassociationPass::Graph;
using Vertex = Graph::vertex_descriptor;
using VertexQueue = circular_buffer<Vertex>;
using Map = BooleanReassociationPass::Map;
using EdgeQueue = std::queue<std::pair<Vertex, Vertex>>;
using TypeId = PabloAST::ClassTypeId;
using DistributionGraph = adjacency_list<hash_setS, vecS, bidirectionalS, Vertex>;
using DistributionMap = flat_map<Graph::vertex_descriptor, DistributionGraph::vertex_descriptor>;
using VertexSet = std::vector<Vertex>;
using VertexSets = std::vector<VertexSet>;
using Biclique = std::pair<VertexSet, VertexSet>;
using BicliqueSet = std::vector<Biclique>;
using DistributionSet = std::tuple<std::vector<Vertex>, std::vector<Vertex>, std::vector<Vertex>>;
using DistributionSets = std::vector<DistributionSet>;

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief helper functions
 ** ------------------------------------------------------------------------------------------------------------- */
template <class Graph>
static VertexSet incomingVertexSet(const Vertex u, const Graph & G) {
    VertexSet V;
    V.reserve(G.in_degree(u));
    for (auto e : make_iterator_range(G.in_edges(u))) {
        V.push_back(G.source(e));
    }
    std::sort(V.begin(), V.end());
    return std::move(V);
}

template <class Graph>
static VertexSet outgoingVertexSet(const Vertex u, const Graph & G) {
    VertexSet V;
    V.reserve(G.out_degree(u));
    for (auto e : make_iterator_range(G.out_edges(u))) {
        V.push_back(G.target(e));
    }
    std::sort(V.begin(), V.end());
    return std::move(V);
}

template <class Graph>
static VertexSet sinks(const Graph & G) {
    VertexSet V;
    for (const Vertex u : make_iterator_range(vertices(G))) {
        if (out_degree(u, G) == 0) {
            V.push_back(u);
        }
    }
    std::sort(V.begin(), V.end());
    return std::move(V);
}

template<typename Iterator>
inline Graph::edge_descriptor first(const std::pair<Iterator, Iterator> & range) {
    assert (range.first != range.second);
    return *range.first;
}

inline void add_edge(PabloAST * expr, const Vertex u, const Vertex v, Graph & G) {
    G[add_edge(u, v, G).first] = expr;
}

static unsigned distributionCount = 0;
/** ------------------------------------------------------------------------------------------------------------- *
 * @brief optimize
 ** ------------------------------------------------------------------------------------------------------------- */
bool BooleanReassociationPass::optimize(PabloFunction & function) {
    BooleanReassociationPass brp;
    brp.resolveScopes(function);
    brp.processScopes(function);
    Simplifier::optimize(function);
    return true;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief resolveScopes
 ** ------------------------------------------------------------------------------------------------------------- */
inline void BooleanReassociationPass::resolveScopes(PabloFunction &function) {
    mResolvedScopes.emplace(&function.getEntryBlock(), nullptr);
    resolveScopes(function.getEntryBlock());
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief resolveScopes
 ** ------------------------------------------------------------------------------------------------------------- */
void BooleanReassociationPass::resolveScopes(PabloBlock & block) {
    for (Statement * stmt : block) {
        if (isa<If>(stmt) || isa<While>(stmt)) {
            PabloBlock & nested = isa<If>(stmt) ? cast<If>(stmt)->getBody() : cast<While>(stmt)->getBody();
            mResolvedScopes.emplace(&nested, stmt);
            resolveScopes(nested);
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief processScopes
 ** ------------------------------------------------------------------------------------------------------------- */
inline void BooleanReassociationPass::processScopes(PabloFunction & function) {
    processScopes(function, function.getEntryBlock());
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief processScopes
 ** ------------------------------------------------------------------------------------------------------------- */
void BooleanReassociationPass::processScopes(PabloFunction & function, PabloBlock & block) {
    for (Statement * stmt : block) {
        if (isa<If>(stmt)) {
            processScopes(function, cast<If>(stmt)->getBody());
        } else if (isa<While>(stmt)) {
            processScopes(function, cast<While>(stmt)->getBody());
        }
    }
    processScope(function, block);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief isOptimizable
 *
 * And, Or and Xor instructions are all associative, commutative and distributive operations. Thus we can
 * safely rearrange expressions such as "((((a ∨ b) ∨ c) ∨ d) ∨ e) ∨ f" into "((a ∨ b) ∨ (c ∨ d)) ∨ (e ∨ f)".
 ** ------------------------------------------------------------------------------------------------------------- */
static inline bool isOptimizable(const PabloAST * const expr) {
    assert (expr);
    switch (expr->getClassTypeId()) {
        case PabloAST::ClassTypeId::And:
        case PabloAST::ClassTypeId::Or:
        case PabloAST::ClassTypeId::Xor:
            return true;
        default:
            return false;
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief inCurrentBlock
 ** ------------------------------------------------------------------------------------------------------------- */
static inline bool inCurrentBlock(const Statement * stmt, const PabloBlock & block) {
    return stmt->getParent() == &block;
}

static inline bool inCurrentBlock(const PabloAST * expr, const PabloBlock & block) {
    return isa<Statement>(expr) && inCurrentBlock(cast<Statement>(expr), block);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getVertex
 ** ------------------------------------------------------------------------------------------------------------- */
template<typename ValueType, typename GraphType, typename MapType>
static inline Vertex getVertex(const ValueType value, GraphType & G, MapType & M) {
    const auto f = M.find(value);
    if (f != M.end()) {
        return f->second;
    }
    const auto u = add_vertex(value, G);
    M.insert(std::make_pair(value, u));
    return u;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief printGraph
 ** ------------------------------------------------------------------------------------------------------------- */
static void printGraph(const PabloBlock & block, const Graph & G, const std::string name) {
    raw_os_ostream out(std::cerr);

    std::vector<unsigned> component(num_vertices(G));
    strong_components(G, make_iterator_property_map(component.begin(), get(vertex_index, G)));

    std::vector<Vertex> ordering;
    ordering.reserve(num_vertices(G));
    topological_sort(G, std::back_inserter(ordering));
    std::reverse(ordering.begin(), ordering.end());

    out << "digraph " << name << " {\n";
    for (auto u : ordering) {
        if (in_degree(u, G) == 0 && out_degree(u, G) == 0) {
            continue;
        }
        out << "v" << u << " [label=\"";
        PabloAST * expr = G[u];
        if (isa<Statement>(expr)) {
            if (LLVM_UNLIKELY(isa<If>(expr))) {
                out << "If ";
                PabloPrinter::print(cast<If>(expr)->getOperand(0), out);
                out << ":";
            } else if (LLVM_UNLIKELY(isa<While>(expr))) {
                out << "While ";
                PabloPrinter::print(cast<While>(expr)->getOperand(0), out);
                out << ":";
            } else {
                PabloPrinter::print(cast<Statement>(expr), "", out);
            }
        } else {
            PabloPrinter::print(expr, out);
        }
        out << "\"";
        if (!inCurrentBlock(expr, block)) {
            out << " style=dashed";
        }
        out << "];\n";
    }

    for (auto e : make_iterator_range(edges(G))) {
        const auto i = source(e, G), j = target(e, G);
        out << "v" << i << " -> v" << j;
        if (LLVM_UNLIKELY(component[i] == component[j])) {
            out << "[color=red]";
        }
        out << ";\n";
    }

    if (num_vertices(G) > 0) {

        out << "{ rank=same;";
        for (auto u : ordering) {
            if (in_degree(u, G) == 0 && out_degree(u, G) != 0) {
                out << " v" << u;
            }
        }
        out << "}\n";

        out << "{ rank=same;";
        for (auto u : ordering) {
            if (out_degree(u, G) == 0 && in_degree(u, G) != 0) {
                out << " v" << u;
            }
        }
        out << "}\n";

    }

    out << "}\n\n";
    out.flush();
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief printGraph
 ** ------------------------------------------------------------------------------------------------------------- */
template<typename SubgraphType>
static void printGraph(const PabloBlock & block, const SubgraphType & S, const Graph & G, const std::string name) {
    raw_os_ostream out(std::cerr);
    out << "digraph " << name << " {\n";
    for (auto u : make_iterator_range(vertices(S))) {
        if (in_degree(u, S) == 0 && out_degree(u, S) == 0) {
            continue;
        }
        out << "v" << u << " [label=\"";
        PabloAST * expr = G[S[u]];
        if (isa<Statement>(expr)) {
            if (LLVM_UNLIKELY(isa<If>(expr))) {
                out << "If ";
                PabloPrinter::print(cast<If>(expr)->getOperand(0), out);
                out << ":";
            } else if (LLVM_UNLIKELY(isa<While>(expr))) {
                out << "While ";
                PabloPrinter::print(cast<While>(expr)->getOperand(0), out);
                out << ":";
            } else {
                PabloPrinter::print(cast<Statement>(expr), "", out);
            }
        } else {
            PabloPrinter::print(expr, out);
        }
        out << "\"";
        if (!inCurrentBlock(expr, block)) {
            out << " style=dashed";
        }
        out << "];\n";
    }
    for (auto e : make_iterator_range(edges(S))) {
        out << "v" << source(e, S) << " -> v" << target(e, S) << ";\n";
    }

    if (num_vertices(S) > 0) {

        out << "{ rank=same;";
        for (auto u : make_iterator_range(vertices(S))) {
            if (in_degree(u, S) == 0 && out_degree(u, S) != 0) {
                out << " v" << u;
            }
        }
        out << "}\n";

        out << "{ rank=same;";
        for (auto u : make_iterator_range(vertices(S))) {
            if (out_degree(u, S) == 0 && in_degree(u, S) != 0) {
                out << " v" << u;
            }
        }
        out << "}\n";

    }

    out << "}\n\n";
    out.flush();
}


/** ------------------------------------------------------------------------------------------------------------- *
 * @brief createTree
 ** ------------------------------------------------------------------------------------------------------------- */
static PabloAST * createTree(PabloBlock & block, const PabloAST::ClassTypeId typeId, circular_buffer<PabloAST *> & Q) {
    assert (Q.size() > 1);
    while (Q.size() > 1) {
        PabloAST * e1 = Q.front(); Q.pop_front();
        PabloAST * e2 = Q.front(); Q.pop_front();
        PabloAST * expr = nullptr;
        // Note: this switch ought to compile to an array of function pointers to the appropriate method.
        switch (typeId) {
            case PabloAST::ClassTypeId::And:
                expr = block.createAnd(e1, e2); break;
            case PabloAST::ClassTypeId::Or:
                expr = block.createOr(e1, e2); break;
            case PabloAST::ClassTypeId::Xor:
                expr = block.createXor(e1, e2); break;
            default: break;
        }
        Q.push_back(expr);
    }
    PabloAST * const expr = Q.front();
    Q.clear();
    return expr;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief processScope
 ** ------------------------------------------------------------------------------------------------------------- */
inline void BooleanReassociationPass::processScope(PabloFunction & function, PabloBlock & block) {
    Graph G;

    summarizeAST(block, G);
    assert (summarizeGraph(block, G) == false);
    redistributeAST(block, G);
    assert (summarizeGraph(block, G) == false);

    raw_os_ostream out(std::cerr);
    out << "======================================================\n";
    PabloPrinter::print(block, " > ", out);
    out << "------------------------------------------------------\n";
    out.flush();

    printGraph(block, G, "G");

    out << "------------------------------------------------------\n";

    circular_buffer<Vertex> Q(num_vertices(G));
    topological_sort(G, std::back_inserter(Q));
    block.setInsertPoint(nullptr);

    while (!Q.empty()) {
        const Vertex u = Q.back(); Q.pop_back();
        if (LLVM_LIKELY(in_degree(u, G) != 0 || out_degree(u, G) != 0)) {
            if (LLVM_LIKELY(inCurrentBlock(G[u], block))) {
                Statement * stmt = cast<Statement>(G[u]);
                PabloPrinter::print(stmt, " - ", out); out << '\n';
                if (isOptimizable(stmt)) {
                    circular_buffer<PabloAST *> nodes(in_degree(u, G));
                    for (auto e : make_iterator_range(in_edges(u, G))) {
                        nodes.push_back(G[source(e, G)]);
                    }
                    Statement * replacement = cast<Statement>(createTree(block, stmt->getClassTypeId(), nodes));
                    stmt->replaceWith(replacement, false, true);
                    G[u] = stmt = replacement;
                    PabloPrinter::print(replacement, "   -> replaced with ", out); out << '\n';
                } else {
                    for (const auto e : make_iterator_range(in_edges(u, G))) {
                        const auto v = source(e, G);
                        if (LLVM_UNLIKELY(G[v] != G[e])) {



                        }
                    }
                }
                block.insert(stmt);
            }
        }
    }

    out << "------------------------------------------------------\n";
    PabloPrinter::print(block, " < ", out);
    out << "------------------------------------------------------\n";
    out.flush();

    PabloVerifier::verify(function);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief summarizeAST
 *
 * This function scans through a basic block (starting by its terminals) and computes a DAG in which any sequences
 * of AND, OR or XOR functions are "flattened" (i.e., allowed to have any number of inputs.) This allows us to
 * reassociate them in the most efficient way possible.
 ** ------------------------------------------------------------------------------------------------------------- */
void BooleanReassociationPass::summarizeAST(PabloBlock & block, Graph & G) const {
    Map M;
    // Compute the base def-use graph ...
    for (Statement * stmt : block) {
        const Vertex u = getVertex(stmt, G, M);
        for (unsigned i = 0; i != stmt->getNumOperands(); ++i) {
            PabloAST * const op = stmt->getOperand(i);
            if (LLVM_UNLIKELY(isa<Integer>(op) || isa<String>(op))) {
                continue;
            }
            add_edge(op, getVertex(op, G, M), u, G);
        }
        if (isa<If>(stmt)) {
            for (Assign * def : cast<const If>(stmt)->getDefined()) {
                const Vertex v = getVertex(def, G, M);
                add_edge(def, u, v, G);
                resolveUsages(v, def, block, G, M, stmt);
            }
        } else if (isa<While>(stmt)) {
            for (Next * var : cast<const While>(stmt)->getVariants()) {
                const Vertex v = getVertex(var, G, M);
                add_edge(var, u, v, G);
                resolveUsages(v, var, block, G, M, stmt);
            }
        } else {
            resolveUsages(u, stmt, block, G, M);
        }
    }
    summarizeGraph(block, G);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief resolveUsages
 ** ------------------------------------------------------------------------------------------------------------- */
bool BooleanReassociationPass::summarizeGraph(PabloBlock & block, Graph & G) {
    bool modified = false;
    std::vector<Vertex> reverseTopologicalOrdering;
    reverseTopologicalOrdering.reserve(num_vertices(G));
    topological_sort(G, std::back_inserter(reverseTopologicalOrdering));
    for (const Vertex u : reverseTopologicalOrdering) {
        if (isOptimizable(G[u]) && inCurrentBlock(G[u], block)) {
            if (LLVM_UNLIKELY(in_degree(u, G) == 1)) {
                // We have a redundant node here that'll simply end up being a duplicate
                // of the input value. Remove it and add any of its outgoing edges to its
                // input node.
                const auto ei = first(in_edges(u, G));
                const Vertex v = source(ei, G);
                for (auto ej : make_iterator_range(out_edges(u, G))) {
                    add_edge(G[ei], v, target(ej, G), G);
                }
                clear_vertex(u, G);
                modified = true;
                continue;
            } else if (LLVM_UNLIKELY(out_degree(u, G) == 1)) {
                // Otherwise if we have a single user, we have a similar case as above but
                // we can only merge this vertex into the outgoing instruction if they are
                // of the same type.
                const auto ei = first(out_edges(u, G));
                const Vertex v = target(ei, G);
                if (LLVM_UNLIKELY(G[v]->getClassTypeId() == G[u]->getClassTypeId())) {
                    for (auto ej : make_iterator_range(in_edges(u, G))) {
                        add_edge(G[ei], source(ej, G), v, G);
                    }
                    clear_vertex(u, G);
                    modified = true;
                    continue;
                }
            }
        }
        // Finally, eliminate any unnecessary instruction
        if (LLVM_UNLIKELY(out_degree(u, G) == 0 && !(isa<Assign>(G[u]) || isa<Next>(G[u])))) {
            clear_in_edges(u, G);
        }
    }
    assert (modified ? (summarizeGraph(block, G) == false) : true);
    return modified;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief resolveUsages
 ** ------------------------------------------------------------------------------------------------------------- */
void BooleanReassociationPass::resolveUsages(const Vertex u, PabloAST * expr, PabloBlock & block, Graph & G, Map & M, Statement * ignoreIfThis) const {
    for (PabloAST * user : expr->users()) {
        assert (user);
        if (LLVM_LIKELY(user != expr && isa<Statement>(user))) {
            PabloBlock * parent = cast<Statement>(user)->getParent();
            assert (parent);
            if (LLVM_UNLIKELY(parent != &block)) {
                for (;;) {
                    if (LLVM_UNLIKELY(parent == nullptr)) {
                        assert (isa<Assign>(expr) || isa<Next>(expr));
                        break;
                    } else if (parent->getParent() == &block) {
                        const auto f = mResolvedScopes.find(parent);
                        if (LLVM_UNLIKELY(f == mResolvedScopes.end())) {
                            throw std::runtime_error("Failed to resolve scope block!");
                        }
                        if (LLVM_UNLIKELY(f->second == ignoreIfThis)) {
                            break;
                        }
                        add_edge(u, getVertex(f->second, G, M), G);
                        break;
                    }
                    parent = parent->getParent();
                }
            }
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief enumerateBicliques
 *
 * Adaptation of the MICA algorithm as described in "Consensus algorithms for the generation of all maximal
 * bicliques" by Alexe et. al. (2003). Note: this implementation considers all verticies with an in-degree of 0
 * to be in bipartition A and their adjacencies to be in B.
  ** ------------------------------------------------------------------------------------------------------------- */
template <class Graph>
static BicliqueSet enumerateBicliques(const Graph & G, const VertexSet & A) {
    using IntersectionSets = std::set<VertexSet>;

    IntersectionSets B1;
    for (auto u : A) {
        B1.insert(std::move(incomingVertexSet(u, G)));
    }

    IntersectionSets B(B1);

    IntersectionSets Bi;

    VertexSet clique;
    for (auto i = B1.begin(); i != B1.end(); ++i) {
        for (auto j = i; ++j != B1.end(); ) {
            std::set_intersection(i->begin(), i->end(), j->begin(), j->end(), std::back_inserter(clique));
            if (clique.size() > 0) {
                if (B.count(clique) == 0) {
                    Bi.insert(clique);
                }
                clique.clear();
            }
        }
    }

    for (;;) {
        if (Bi.empty()) {
            break;
        }
        B.insert(Bi.begin(), Bi.end());
        IntersectionSets Bk;
        for (auto i = B1.begin(); i != B1.end(); ++i) {
            for (auto j = Bi.begin(); j != Bi.end(); ++j) {
                std::set_intersection(i->begin(), i->end(), j->begin(), j->end(), std::back_inserter(clique));
                if (clique.size() > 0) {
                    if (B.count(clique) == 0) {
                        Bk.insert(clique);
                    }
                    clique.clear();
                }
            }
        }
        Bi.swap(Bk);
    }

    BicliqueSet cliques;
    cliques.reserve(B.size());
    for (auto Bi = B.begin(); Bi != B.end(); ++Bi) {
        VertexSet Ai(A);
        for (const Vertex u : *Bi) {
            VertexSet Aj = outgoingVertexSet(u, G);
            VertexSet Ak;
            std::set_intersection(Ai.begin(), Ai.end(), Aj.begin(), Aj.end(), std::back_inserter(Ak));
            Ai.swap(Ak);
        }
        cliques.emplace_back(std::move(Ai), std::move(*Bi));
    }
    return std::move(cliques);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief intersects
 ** ------------------------------------------------------------------------------------------------------------- */
template <class Type>
inline bool intersects(const Type & A, const Type & B) {
    auto first1 = A.begin(), last1 = A.end();
    auto first2 = B.begin(), last2 = B.end();
    while (first1 != last1 && first2 != last2) {
        if (*first1 < *first2) {
            ++first1;
        } else if (*first2 < *first1) {
            ++first2;
        } else {
            return true;
        }
    }
    return false;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief independentCliqueSets
 ** ------------------------------------------------------------------------------------------------------------- */
template <unsigned side>
inline static BicliqueSet && independentCliqueSets(BicliqueSet && cliques, const unsigned minimum) {
    using IndependentSetGraph = adjacency_list<hash_setS, vecS, undirectedS, unsigned>;

    const auto l = cliques.size();
    IndependentSetGraph I(l);

    // Initialize our weights
    for (unsigned i = 0; i != l; ++i) {
        I[i] = std::pow(std::get<side>(cliques[i]).size(), 2);
    }

    // Determine our constraints
    for (unsigned i = 0; i != l; ++i) {
        for (unsigned j = i + 1; j != l; ++j) {
            if (intersects(std::get<side>(cliques[i]), std::get<side>(cliques[j]))) {
                add_edge(i, j, I);
            }
        }
    }

    // Use the greedy algorithm to choose our independent set
    VertexSet selected;
    for (;;) {
        unsigned w = 0;
        Vertex u = 0;
        for (unsigned i = 0; i != l; ++i) {
            if (I[i] > w) {
                w = I[i];
                u = i;
            }
        }
        if (w < minimum) break;
        selected.push_back(u);
        I[u] = 0;
        for (auto v : make_iterator_range(adjacent_vertices(u, I))) {
            I[v] = 0;
        }
    }

    // Sort the selected list and then remove the unselected cliques
    std::sort(selected.begin(), selected.end(), std::greater<Vertex>());
    auto end = cliques.end();
    for (const unsigned offset : selected) {
        end = cliques.erase(cliques.begin() + offset + 1, end) - 1;
    }
    cliques.erase(cliques.begin(), end);

    return std::move(cliques);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief removeUnhelpfulBicliques
 *
 * An intermediary vertex could have more than one outgoing edge but if any edge is not directed to a vertex in our
 * biclique, we'll need to compute that specific value anyway. Remove them from the clique set and if there are not
 * enough vertices in the biclique to make distribution profitable, eliminate the clique.
 ** ------------------------------------------------------------------------------------------------------------- */
static BicliqueSet && removeUnhelpfulBicliques(BicliqueSet && cliques, const Graph & G, DistributionGraph & H) {
    for (auto ci = cliques.begin(); ci != cliques.end(); ) {
        const auto cardinalityA = std::get<0>(*ci).size();
        VertexSet & B = std::get<1>(*ci);
        for (auto bi = B.begin(); bi != B.end(); ) {
            if (out_degree(H[*bi], G) == cardinalityA) {
                ++bi;
            } else {
                bi = B.erase(bi);
            }
        }
        if (B.size() > 1) {
            ++ci;
        } else {
            ci = cliques.erase(ci);
        }
    }
    return std::move(cliques);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief safeDistributionSets
 ** ------------------------------------------------------------------------------------------------------------- */
static DistributionSets safeDistributionSets(const PabloBlock & block, const Graph & G, DistributionGraph & H) {
    const auto F = makeGraphFacade(H);
    DistributionSets T;
    BicliqueSet lowerSet = independentCliqueSets<1>(removeUnhelpfulBicliques(enumerateBicliques(F, sinks(H)), G, H), 1);
    for (Biclique & lower : lowerSet) {
        BicliqueSet upperSet = independentCliqueSets<0>(enumerateBicliques(F, std::get<1>(lower)), 2);
        for (Biclique & upper : upperSet) {
            T.emplace_back(std::move(std::get<1>(upper)), std::move(std::get<0>(upper)), std::get<0>(lower));
        }
    }
    return std::move(T);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief generateDistributionGraph
 ** ------------------------------------------------------------------------------------------------------------- */
void generateDistributionGraph(const PabloBlock & block, const Graph & G, DistributionGraph & H) {
    DistributionMap M;
    for (const Vertex u : make_iterator_range(vertices(G))) {
        if (in_degree(u, G) > 0) {
            const TypeId outerTypeId = G[u]->getClassTypeId();
            if (outerTypeId == TypeId::And || outerTypeId == TypeId::Or) {
                if (inCurrentBlock(cast<Statement>(G[u]), block)) {
                    const TypeId innerTypeId = (outerTypeId == TypeId::And) ? TypeId::Or : TypeId::And;
                    flat_set<Vertex> distributable;
                    for (auto e : make_iterator_range(in_edges(u, G))) {
                        const Vertex v = source(e, G);
                        if (LLVM_UNLIKELY(G[v]->getClassTypeId() == innerTypeId && inCurrentBlock(cast<Statement>(G[v]), block))) {
                            bool safe = true;
                            for (const auto e : make_iterator_range(out_edges(v, G))) {
                                if (G[target(e, G)]->getClassTypeId() != outerTypeId) {
                                    safe = false;
                                    break;
                                }
                            }
                            if (safe) {
                                distributable.insert(v);
                            }
                        }
                    }
                    if (LLVM_LIKELY(distributable.size() > 1)) {
                        // We're only interested in computing a subgraph of G in which every source has an out-degree
                        // greater than 1. Otherwise we'd only end up permuting the AND/OR sequence with no logical
                        // benefit. (Note: this does not consider register pressure / liveness.)
                        flat_map<Vertex, bool> observedMoreThanOnce;
                        bool anyOpportunities = false;
                        for (const Vertex v : distributable) {
                            for (auto e : make_iterator_range(in_edges(v, G))) {
                                const Vertex w = source(e, G);
                                auto ob = observedMoreThanOnce.find(w);
                                if (ob == observedMoreThanOnce.end()) {
                                    observedMoreThanOnce.emplace(w, false);
                                } else {
                                    ob->second = true;
                                    anyOpportunities = true;
                                }
                            }
                        }
                        if (anyOpportunities) {
                            for (const auto ob : observedMoreThanOnce) {
                                if (ob.second) {
                                    const Vertex w = ob.first;
                                    for (auto e : make_iterator_range(out_edges(w, G))) {
                                        const Vertex v = target(e, G);
                                        if (distributable.count(v)) {
                                            const Vertex y = getVertex(v, H, M);
                                            add_edge(y, getVertex(u, H, M), H);
                                            add_edge(getVertex(w, H, M), y, H);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief redistributeAST
 *
 * Apply the distribution law to reduce computations whenever possible.
 ** ------------------------------------------------------------------------------------------------------------- */
bool BooleanReassociationPass::redistributeAST(PabloBlock & block, Graph & G) const {
    bool anyChanges = false;

    ++distributionCount;

    for (;;) {

        DistributionGraph H;

        generateDistributionGraph(block, G, H);

        // If we found no potential opportunities then we cannot apply the distribution law to any part of G.
        if (num_vertices(H) == 0) {
            break;
        }

        const DistributionSets distributionSets = safeDistributionSets(block, G, H);

        if (LLVM_UNLIKELY(distributionSets.empty())) {
            break;
        }

        for (const DistributionSet & set : distributionSets) {
            // Each distribution tuple consists of the sources, intermediary, and sink nodes.
            const VertexSet & sources = std::get<0>(set);
            const VertexSet & intermediary = std::get<1>(set);
            const VertexSet & sinks = std::get<2>(set);

            const TypeId outerTypeId = G[H[sinks.front()]]->getClassTypeId();
            assert (outerTypeId == TypeId::And || outerTypeId == TypeId::Or);
            const TypeId innerTypeId = (outerTypeId == TypeId::Or) ? TypeId::And : TypeId::Or;

            // Eliminate the edges from our original graph G
            for (const Vertex u : intermediary) {
                const auto x = H[u];
                assert (G[x]->getClassTypeId() == innerTypeId);
                for (const Vertex s : sources) {
                    assert (edge(H[s], x, G).second);
                    remove_edge(H[s], x, G);
                }
                for (const Vertex t : sinks) {
                    assert (edge(x, H[t], G).second);
                    assert (G[H[t]]->getClassTypeId() == outerTypeId);
                    remove_edge(x, H[t], G);
                }
            }

            // Update G to match the desired changes
            const Vertex x = add_vertex(nullptr, G);
            const Vertex y = add_vertex(nullptr, G);
            for (const Vertex u : intermediary) {
                add_edge(nullptr, H[u], x, G);
            }
            for (const Vertex s : sources) {
                add_edge(nullptr, H[s], y, G);
            }
            for (const Vertex t : sinks) {
                add_edge(nullptr, y, H[t], G);
            }
            add_edge(nullptr, x, y, G);

            // Now begin transforming the AST (TODO: fix the abstraction so I can defer creation until the final phase)
            circular_buffer<PabloAST *> Q(std::max(in_degree(x, G), in_degree(y, G)));
            for (auto e : make_iterator_range(in_edges(x, G))) {
                Q.push_back(G[source(e, G)]);
            }
            G[x] = createTree(block, outerTypeId, Q);
            for (auto e : make_iterator_range(in_edges(y, G))) {
                Q.push_back(G[source(e, G)]);
            }
            G[y] = createTree(block, innerTypeId, Q);

            // Summarize the graph after transforming G
            summarizeGraph(block, G);
        }
        anyChanges = true;
    }
    return anyChanges;
}

}
