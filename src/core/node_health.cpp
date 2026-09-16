#include "efis/core/node_health.hpp"

#include <algorithm>
#include <utility>

namespace efis {

NodeHealthMonitor::NodeHealthMonitor(NodeHealthPolicy policy) noexcept
    : policy_(policy) {}

void NodeHealthMonitor::declare(NodeId node) {
    if (node == node_undeclared) return;
    if (find(node) != nullptr) return;
    NodeStatus status{};
    status.node = node;
    status.state = NodeState::unknown;
    nodes_.push_back(status);
}

NodeStatus* NodeHealthMonitor::find(NodeId node) noexcept {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [node](const NodeStatus& s) { return s.node == node; });
    return it == nodes_.end() ? nullptr : &*it;
}

HeartbeatVerdict NodeHealthMonitor::heartbeat(NodeId node, MonoTime at,
                                              std::uint64_t sequence) {
    // Un nodo che si presenta senza essere stato dichiarato viene comunque
    // seguito: ignorarlo nasconderebbe un nodo realmente presente sul bus, che
    // e' un'anomalia da mostrare e non da sopprimere.
    declare(node);
    NodeStatus* status = find(node);
    if (status == nullptr) return HeartbeatVerdict::regressed;

    if (status->accepted == 0) {
        status->accepted = 1;
        status->last_sequence = sequence;
        status->last_accepted = at;
        status->state = NodeState::online;
        return HeartbeatVerdict::accepted;
    }

    if (sequence == status->last_sequence) {
        ++status->duplicates;
        return HeartbeatVerdict::duplicate;
    }
    if (sequence < status->last_sequence) {
        ++status->regressions;
        return HeartbeatVerdict::regressed;
    }

    const NodeState previous = status->state;
    const std::uint64_t gap = sequence - status->last_sequence - 1;
    ++status->accepted;
    status->last_sequence = sequence;
    status->last_accepted = at;
    status->state = NodeState::online;
    if (previous == NodeState::stale || previous == NodeState::lost) {
        pending_changes_.push_back({node, previous, NodeState::online, at});
    }
    if (gap > 0) {
        status->missed += gap;
        return HeartbeatVerdict::accepted_after_gap;
    }
    return HeartbeatVerdict::accepted;
}

std::vector<NodeStateChange> NodeHealthMonitor::update(MonoTime now) {
    // Le riprese sono prodotte dal battito, i peggioramenti dal trascorrere del
    // tempo. Restituiamo entrambi nello stesso canale affinche' adapter,
    // recorder e diagnostica non possano perdere meta' della sequenza.
    std::vector<NodeStateChange> changes = std::move(pending_changes_);
    pending_changes_.clear();
    for (NodeStatus& status : nodes_) {
        // Un nodo mai sentito resta `unknown`: il tempo trascorso non lo rende
        // "perso", perche' non c'e' stato nulla da perdere. Uscira' da questo
        // stato soltanto al primo battito accettato.
        if (status.accepted == 0) continue;

        const auto silence = now - status.last_accepted;
        NodeState next = NodeState::online;
        if (silence >= policy_.lost_after) {
            next = NodeState::lost;
        } else if (silence >= policy_.stale_after) {
            next = NodeState::stale;
        }

        if (next != status.state) {
            changes.push_back({status.node, status.state, next, now});
            status.state = next;
        }
    }
    return changes;
}

std::optional<NodeStatus> NodeHealthMonitor::status(NodeId node) const {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [node](const NodeStatus& s) { return s.node == node; });
    if (it == nodes_.end()) return std::nullopt;
    return *it;
}

bool NodeHealthMonitor::all_online() const noexcept {
    return !nodes_.empty() &&
           std::all_of(nodes_.begin(), nodes_.end(),
                       [](const NodeStatus& s) { return is_usable(s.state); });
}

}  // namespace efis
