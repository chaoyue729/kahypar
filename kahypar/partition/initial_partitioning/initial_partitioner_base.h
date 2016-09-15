/***************************************************************************
 *  Copyright (C) 2015 Tobias Heuer <tobias.heuer@gmx.net>
 **************************************************************************/

#pragma once

#include <algorithm>
#include <limits>
#include <map>
#include <stack>
#include <vector>

#include "definitions.h"
#include "partition/configuration.h"
#include "partition/factories.h"
#include "partition/metrics.h"
#include "partition/refinement/i_refiner.h"
#include "partition/refinement/kway_fm_cut_refiner.h"
#include "partition/refinement/policies/fm_improvement_policy.h"
#include "partition/refinement/policies/fm_stop_policy.h"

using partition::Configuration;
using partition::StoppingPolicy;
using partition::NumberOfFruitlessMovesStopsSearch;
using partition::RandomWalkModelStopsSearch;
using partition::nGPRandomWalkStopsSearch;
using partition::RefinerFactory;
using partition::KWayFMRefiner;
using partition::IRefiner;

namespace partition {
class InitialPartitionerBase {
 protected:
  static constexpr PartitionID kInvalidPart = std::numeric_limits<PartitionID>::max();
  static constexpr HypernodeID kInvalidNode = std::numeric_limits<HypernodeID>::max();

 public:
  InitialPartitionerBase(Hypergraph& hypergraph, Configuration& config) noexcept :
    _hg(hypergraph),
    _config(config),
    _unassigned_nodes(),
    _unassigned_node_bound(std::numeric_limits<PartitionID>::max()),
    _max_hypernode_weight(hypergraph.weightOfHeaviestNode()) {
    for (const HypernodeID hn : _hg.nodes()) {
      _unassigned_nodes.push_back(hn);
    }
    _unassigned_node_bound = _unassigned_nodes.size();
  }

  virtual ~InitialPartitionerBase() { }

  void recalculateBalanceConstraints(const double epsilon) {
    for (int i = 0; i < _config.initial_partitioning.k; ++i) {
      _config.initial_partitioning.upper_allowed_partition_weight[i] =
        _config.initial_partitioning.perfect_balance_partition_weight[i]
        * (1.0 + epsilon);
    }
    _config.partition.max_part_weights[0] =
      _config.initial_partitioning.upper_allowed_partition_weight[0];
    _config.partition.max_part_weights[1] =
      _config.initial_partitioning.upper_allowed_partition_weight[1];
  }

  void resetPartitioning() {
    _hg.resetPartitioning();
    if (_config.initial_partitioning.unassigned_part != -1) {
      for (const HypernodeID hn : _hg.nodes()) {
        _hg.setNodePart(hn, _config.initial_partitioning.unassigned_part);
      }
      _hg.initializeNumCutHyperedges();
    }
    _unassigned_node_bound = _unassigned_nodes.size();
  }

  void performFMRefinement() {
    if (_config.initial_partitioning.refinement) {
      std::unique_ptr<IRefiner> refiner;
      if (_config.local_search.algorithm == RefinementAlgorithm::twoway_fm &&
          _config.initial_partitioning.k > 2) {
        refiner = (RefinerFactory::getInstance().createObject(
                     RefinementAlgorithm::kway_fm,
                     _hg, _config));
        LOG("WARNING: Trying to use twoway_fm for k > 2! Refiner is set to kway_fm.");
      } else {
        refiner = (RefinerFactory::getInstance().createObject(
                     _config.local_search.algorithm,
                     _hg, _config));
      }

      refiner->initialize();
      std::vector<HypernodeID> refinement_nodes;
      Metrics current_metrics = { metrics::hyperedgeCut(_hg),
                                  metrics::km1(_hg),
                                  metrics::imbalance(_hg, _config) };

#ifndef NDEBUG
      HyperedgeWeight old_cut = current_metrics.cut;
#endif

      bool improvement_found = false;
      int iteration = 0;

      UncontractionGainChanges changes;
      changes.representative.push_back(0);
      changes.contraction_partner.push_back(0);

      do {
        refinement_nodes.clear();
        for (const HypernodeID hn : _hg.nodes()) {
          if (_hg.isBorderNode(hn)) {
            refinement_nodes.push_back(hn);
          }
        }

        if (refinement_nodes.size() < 2) {
          break;
        }
        improvement_found =
          refiner->refine(refinement_nodes,
                          { _config.initial_partitioning.upper_allowed_partition_weight[0]
                            + _max_hypernode_weight,
                            _config.initial_partitioning.upper_allowed_partition_weight[1]
                            + _max_hypernode_weight }, changes, current_metrics);
        ASSERT(current_metrics.cut <= old_cut, "Cut increased during uncontraction");
        ASSERT(current_metrics.cut == metrics::hyperedgeCut(_hg), "Inconsistent cut values");
#ifndef NDEBUG
        old_cut = current_metrics.cut;
#endif
        ++iteration;
      } while (iteration < _config.initial_partitioning.local_search.iterations_per_level &&
               improvement_found);
    }
  }


  bool assignHypernodeToPartition(const HypernodeID hn, const PartitionID target_part) {
    if (_hg.partWeight(target_part) + _hg.nodeWeight(hn)
        <= _config.initial_partitioning.upper_allowed_partition_weight[target_part]) {
      if (_hg.partID(hn) == -1) {
        _hg.setNodePart(hn, target_part);
      } else {
        if (_hg.partID(hn) != target_part) {
          _hg.changeNodePart(hn, _hg.partID(hn), target_part);
        } else {
          return false;
        }
      }
      ASSERT(_hg.partID(hn) == target_part,
             "Assigned partition of Hypernode " << hn << " should be " << target_part
             << ", but currently is " << _hg.partID(hn));
      return true;
    } else {
      return false;
    }
  }

  HypernodeID getUnassignedNode() {
    HypernodeID unassigned_node = kInvalidNode;
    for (size_t i = 0; i < _unassigned_node_bound; ++i) {
      HypernodeID hn = _unassigned_nodes[i];
      if (_hg.partID(hn) == _config.initial_partitioning.unassigned_part) {
        unassigned_node = hn;
        break;
      } else {
        std::swap(_unassigned_nodes[i--], _unassigned_nodes[--_unassigned_node_bound]);
      }
    }
    return unassigned_node;
  }

  HypernodeWeight getMaxHypernodeWeight() {
    return _max_hypernode_weight;
  }

 protected:
  Hypergraph& _hg;
  Configuration& _config;

 private:
  std::vector<HypernodeID> _unassigned_nodes;
  unsigned int _unassigned_node_bound;
  HypernodeWeight _max_hypernode_weight;
};
}  // namespace partition
