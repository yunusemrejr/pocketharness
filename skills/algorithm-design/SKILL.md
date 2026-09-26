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
