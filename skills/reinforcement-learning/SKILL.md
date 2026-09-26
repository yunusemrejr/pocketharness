---
name: reinforcement-learning
description: Engineer RL environments, rollout buffers, reward functions and policy evaluation; use for reinforcement learning, bandits and agent-training correctness.
---

# Reinforcement Learning

Define observation, action, reward, episode boundary and evaluation objective. Check that the observation contains information available at decision time. Document partial observability rather than leaking simulator state.

1. Validate reset/step shapes, dtypes, action bounds, reward scale and seed behavior with the installed environment API. Run a random or simple scripted policy before training.
2. Distinguish environment termination from external truncation. For value targets, mask genuine terminal states; time-limit truncation can still require bootstrapping from the final observation. Check auto-reset wrappers so the next episode's initial observation is not used accidentally.
3. Verify buffer indexing, discounting, advantage/value targets and policy log-probability shapes on a tiny hand-computable trajectory. Check gradient flow and finite values before large runs.
4. Audit reward hacking: a policy may maximize reward while defeating the user objective. Inspect trajectories, action saturation and terminal conditions, not reward curves alone.
5. Compare to the baseline across separate evaluation seeds/tasks without exploration or training updates as appropriate. Record interaction count, wall time and uncertainty; distinguish sample efficiency from throughput.

Example: a timeout is not automatically a losing terminal state. Incorrectly zeroing its bootstrap can bias value learning.

Consult the installed version against the [environment API](https://gymnasium.farama.org/api/env/). Deliver the environment checks, tiny-target test, baseline and honest evaluation results before scaling training.

For a one-step value-target sanity check, run `python3 scripts/check_targets.py` from this skill directory with JSON on stdin: `{"gamma":0.9,"transitions":[{"reward":1,"next_value":10,"terminated":false,"truncated":true}]}`. The target is 10; changing `terminated` to true makes it 1. Compare these independent targets to the learner. This helper does not validate GAE, n-step returns or the learned value function.

> PocketHarness note: this skill's bundled code assets were removed (the harness ships only native C/C++ and bash). Write the equivalent in the project's own language when needed, and prefer `pocket kit` (wav, audio, spring, svg, shot, web) where it covers the job.
