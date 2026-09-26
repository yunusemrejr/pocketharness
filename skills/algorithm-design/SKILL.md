---
name: algorithm-design
description: Choose and verify algorithms and data structures using workload bounds, correctness arguments and complexity; use for search, graph, scheduling or scaling problems.
---

# Algorithm Design

Define input size, shape, ordering, mutation rate, query pattern and required exactness. Establish a simple correct baseline before choosing a sophisticated structure.

1. State the invariant or recurrence and why the algorithm terminates. For graph problems identify directedness, weights, cycles and disconnected cases before choosing traversal or shortest paths.
2. Compare worst-case time, memory and relevant amortized costs. Include preprocessing, copying, allocation and output size; distinguish expected from guaranteed bounds.
3. Validate an optimized implementation against brute force on small generated inputs. Include empty, duplicate, adversarially ordered, extreme and disconnected cases as applicable.
4. Measure representative sizes before adding complexity. Use a library implementation when its semantics and bounds fit; do not build a custom structure solely to demonstrate sophistication.
5. Keep approximate answers explicitly approximate, with an error or quality criterion. Stop when the intended workload and correctness checks are satisfied.

Example: Dijkstra's algorithm requires nonnegative edge weights; a faster implementation does not repair an invalid assumption. An O(n) scan can beat an index for one small query because building the index also costs work.

Deliver the algorithm choice, assumptions, correctness argument, complexity and independent checks. Formal proofs require stated premises; passing random tests is not a proof.

## Runnable sparse-graph example

[shortest_path.cpp](assets/shortest_path.cpp) is a C++17 standard-library implementation with a separate Floyd–Warshall oracle. From this skill's directory:

```bash
c++ -std=c++17 -O2 -Wall -Wextra -Werror assets/shortest_path.cpp -o "$TMPDIR/shortest-path"
"$TMPDIR/shortest-path" --self-test
"$TMPDIR/shortest-path" graph.txt
```

The file starts with `vertex_count edge_count source`, then directed `from to weight` rows. Vertices are zero-based; weights are integers in `0..1000000000000`; limits are 100000 vertices and 1000000 edges. It prints distances or `unreachable`. Parallel edges, zero weights and disconnected vertices are supported; negative weights are rejected. The bounds keep all shortest-path arithmetic representable. The lazy priority queue uses O((V+E) log(V+E)) time and O(V+E) space.

Use breadth-first search for unweighted graphs, a DAG pass when a topological order is available, and an algorithm with negative-cycle handling when negative edges are allowed. For an existing project, adapt its graph representation rather than adding another. Acceptance: the 200 generated-graph comparisons pass, the project's actual graph matches a known route, and invalid inputs fail before publishing a result. Use `pocket kit find`, `sym` and `refs` to locate the existing owner before editing; run `pocket kit` for current syntax.
