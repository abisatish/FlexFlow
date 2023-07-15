#include "utils/graph/multidigraph.h"

namespace FlexFlow {

MultiDiInput get_input(MultiDiEdge const &e) {
  return {e.dst, e.dstIdx};
}

MultiDiOutput get_output(MultiDiEdge const &e) {
  return {e.src, e.srcIdx};
}

MultiDiEdgeQuery
    MultiDiEdgeQuery::with_src_nodes(query_set<Node> const &nodes) const {
  MultiDiEdgeQuery e = *this;
  if (is_matchall(e.srcs)) {
    throw mk_runtime_error("Expected matchall previous value");
  }
  e.srcs = nodes;
  return e;
}

<<<<<<< HEAD
std::ostream& operator<<(std::ostream& os, const MultiDiEdge& edge) {
    return os<<"MultiDiEdge{"<<edge.src.value()<< ","<<edge.dst.value()<<","<<edge.srcIdx.value()<<","<<edge.dstIdx.value()<<"}";
}

MultiDiEdgeQuery::  MultiDiEdgeQuery(
      tl::optional<std::unordered_set<Node>> const &srcs,
      tl::optional<std::unordered_set<Node>> const &dsts,
      tl::optional<std::unordered_set<NodePort>> const &srcIdxs ,
      tl::optional<std::unordered_set<NodePort>> const &dstIdxs )
      :srcs(srcs), dsts(dsts),srcIdxs(srcIdxs), dstIdxs(dstIdxs)
{}

MultiDiEdgeQuery MultiDiEdgeQuery::with_src_node(Node const &n) const {
  return this->with_src_nodes({n});
}


MultiDiEdgeQuery
    MultiDiEdgeQuery::with_dst_nodes(query_set<Node> const &nodes) const {
  MultiDiEdgeQuery e = *this;
  if (is_matchall(e.dsts)) {
    throw mk_runtime_error("Expected matchall previous value");
  }
  e.dsts = nodes;
  return e;
}

MultiDiEuery query_intersection(MultiDiEdgeQuery const &lhs, MultiDiEdgeQuery const &rhs){
  assert (lhs.srcs.has_value() &&dgeQ lhs.dsts.has_value() && rhs.srcs.has_value() && rhs.dsts.has_value());
  tl::optional<std::unordered_set<Node>> srcs = intersection(*lhs.srcs, *rhs.srcs);
  tl::optional<std::unordered_set<Node>> dsts = intersection(*lhs.dsts, *rhs.dsts);
  return MultiDiEdgeQuery(srcs, dsts);
}

MultiDiEdgeQuery MultiDiEdgeQuery::with_dst_node(Node const &n) const {
  return this->with_dst_nodes({n});
}

MultiDiEdgeQuery MultiDiEdgeQuery::with_src_idxs(
    std::unordered_set<NodePort> const &idxs) const {
  MultiDiEdgeQuery e{*this};
  if (e.srcIdxs != tl::nullopt) {
    throw std::runtime_error("expected srcIdxs == tl::nullopt");
  }
  e.srcIdxs = idxs;
  return e;
}

MultiDiEdgeQuery
    MultiDiEdgeQuery::with_dst_idxs(query_set<NodePort> const &idxs) const {
  MultiDiEdgeQuery e = *this;
  if (is_matchall(e.dstIdxs)) {
    throw mk_runtime_error("Expected matchall previous value");
  }
  e.dstIdxs = idxs;
  return e;
}

MultiDiEdgeQuery MultiDiEdgeQuery::all() {
  return {matchall<Node>(),
          matchall<Node>(),
          matchall<NodePort>(),
          matchall<NodePort>()};
}

std::unordered_set<Node> MultiDiGraphView::query_nodes(NodeQuery const & q) const {
  return this->ptr->query_nodes(q);
}

std::unordered_set<MultiDiEdge> MultiDiGraphView::query_edges(MultiDiEdgeQuery const &q) const {
  return this->ptr->query_edges(q);
}

NodePort IMultiDiGraph::add_node_port(){
  NodePort np{this->next_nodeport_idx};
  this->next_nodeport_idx += 1;
  return np;
}

void IMultiDiGraph::add_node_port_unsafe(NodePort const &np) {
  this->next_nodeport_idx = std::max(this->next_nodeport_idx, np.value() + 1);
}

MultiDiGraphView::operator GraphView() const {
  return GraphView(this->ptr);
}

MultiDiGraphView unsafe_create(IMultiDiGraphView const &graphView) {
  std::shared_ptr<IMultiDiGraphView const> ptr((&graphView),
      [](IMultiDiGraphView const *ptr) {});
  return MultiDiGraphView(ptr);
}


void swap(MultiDiGraphView &lhs, MultiDiGraphView &rhs) {
  using std::swap;

  swap(lhs.ptr, rhs.ptr);
}

void swap(MultiDiGraph &lhs, MultiDiGraph &rhs) {
  using std::swap;

  swap(lhs.ptr, rhs.ptr);
}

MultiDiGraph::operator MultiDiGraphView() const {
  return MultiDiGraphView(this->ptr.get());
}

Node MultiDiGraph::add_node() {
  return this->ptr.get_mutable()->add_node();
}

NodePort MultiDiGraph::add_node_port() {
  return this->ptr.get_mutable()->add_node_port();
}

void MultiDiGraph::add_node_port_unsafe(NodePort const & np) {
  return this->ptr.get_mutable()->add_node_port_unsafe(np);
}

void MultiDiGraph::add_node_unsafe(Node const &n) {
  return this->ptr.get_mutable()->add_node_unsafe(n);
}

void MultiDiGraph::remove_node_unsafe(Node const &n) {
  return this->ptr.get_mutable()->remove_node_unsafe(n);
}

void MultiDiGraph::add_edge(MultiDiEdge const &e) {
  return this->ptr.get_mutable()->add_edge(e);
}

void MultiDiGraph::remove_edge(MultiDiEdge const &e) {
  return this->ptr.get_mutable()->remove_edge(e);
}

std::unordered_set<MultiDiEdge>
    MultiDiGraph::query_edges(MultiDiEdgeQuery const &q) const {
      return this->ptr->query_edges(q);
}

std::unordered_set<Node> MultiDiGraph::query_nodes(NodeQuery const & q) const {
  return this->ptr->query_nodes(q);
}

} // namespace FlexFlow
