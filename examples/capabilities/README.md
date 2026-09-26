# Runnable capability examples

These are small executable building blocks, not promises that a model will solve every advanced task. Run the relevant example, adapt it to the actual input contract, and verify the requested result. They require no service accounts or model downloads.

| Task | Start here | Runtime and acceptance |
| --- | --- | --- |
| Directed sparse shortest paths | [C++ source](../../skills/algorithm-design/assets/shortest_path.cpp), [contract](../../skills/algorithm-design/SKILL.md#runnable-sparse-graph-example) | C++17 compiler; compare every source against a separate oracle on 200 generated graphs, then check the real graph |
| Train and evaluate a numeric classifier | [C++ source](../../skills/classical-ml-modeling/assets/logistic_regression.cpp), [contract](../../skills/classical-ml-modeling/SKILL.md#dependency-free-baseline) | C++17 compiler; train-only preprocessing, held-out metrics, prior baseline, invalid-data checks |
| Evaluate a decision threshold offline | [C++ evaluator](../../skills/small-model-engineering/assets/decision_threshold.cpp), [recipe and fixtures](../../skills/small-model-engineering/references/jev-decisions.md) | C++17 compiler; separate calibration/held-out scores, explicit unknown coverage, no network or configuration changes |
| Deterministic browser motion | [editable HTML](../../skills/motion-graphics-production/assets/timeline.html), [rendering contract](../../skills/motion-graphics-production/references/rendering.md) | Existing Chromium and FFmpeg for native video export; inspect frames and playback |

From the repository root:

```bash
work=$(mktemp -d)
c++ -std=c++17 -O2 -Wall -Wextra -Werror skills/algorithm-design/assets/shortest_path.cpp -o "$work/shortest-path"
"$work/shortest-path" --self-test
c++ -std=c++17 -O2 -Wall -Wextra -Werror skills/classical-ml-modeling/assets/logistic_regression.cpp -o "$work/logistic-regression"
"$work/logistic-regression" --self-test
```

The graph program also accepts a text file:

```text
4 4 0
0 1 5
0 2 1
2 1 2
1 3 4
```

Expected distances are `0 0`, `1 3`, `2 1`, `3 7`. The classifier accepts separate numeric training and holdout CSV files; its self-test's generated labels are clearly synthetic. Those scores do not establish real-world generalization, fairness or probability calibration.

Skill assets are installed alongside their guides under the bundled skill directory. Use `skill(action="load", name="algorithm-design")` or `classical-ml-modeling` to find their paths. There is no need to install a new language runtime to run either C++ example. Existing project tools take precedence: inspect the repository, identify the owner, implement the smallest relevant change, and run a meaningful contract check before claiming completion.


For the complete six-second motion + synthesized audio demonstration:

```sh
examples/capabilities/motion/render.sh /tmp/pocket-motion
```

This renders 144 frames at 960×540/24 fps, checks the soundtrack and prints the
actual encoded stream metadata. Open `demo.mp4` and `poster.png` in the output
directory to inspect appearance and playback. The native exporter does not
install Chrome or FFmpeg; `pocket kit probe` reports host capabilities.
