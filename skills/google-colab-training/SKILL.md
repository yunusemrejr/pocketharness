---
name: google-colab-training
description: Prepare and operate Google Colab model-training workflows through notebooks, the official CLI, or local runtimes; handle GPU setup, authentication, staging, checkpoints, and recovery.
---

# Google Colab training

Choose the actual connection mode before writing execution instructions. A notebook file on disk is a prepared deliverable, not a connected runtime. Discover available browser, CLI or MCP capabilities; report which runtime was actually reached. Consumer Colab, Colab Enterprise and a local Jupyter runtime have different identity, billing and storage behavior.

Read only the relevant reference:

- [Connections and identity](references/connections.md): hosted browser, official Colab CLI, MCP, local Docker/Jupyter, Enterprise, secrets and storage authentication.
- [Training operations](references/training-operations.md): repeatable environments, dataset staging, interruption-safe checkpoints, GPU/OOM diagnosis, time and cost estimates.

No preflight notebook is bundled. In an already-connected notebook, this standard-library cell identifies the Python runtime and whether a GPU diagnostic executable is available without installing packages or starting training:

```python
import platform, shutil, subprocess
print(platform.python_version())
gpu = shutil.which("nvidia-smi")
if gpu:
    subprocess.run([gpu, "--query-gpu=name,memory.total", "--format=csv,noheader"], check=True, timeout=10)
else:
    print("nvidia-smi unavailable; GPU capability has not been verified")
```

For a training notebook, keep configuration, environment setup, data validation, short training smoke test, full training, evaluation and export as independently rerunnable cells. Keep the executable training logic in a versioned Python module where practical. Verify the selected model's license/access and actual GPU memory before selecting full tuning versus adapters. LoRA/QLoRA reduce trainable state; long sequences and activations can still exceed memory.

Prepare and validate local artifacts before cloud execution. Existing user authorization remains sufficient within its scope; skill installation itself does not authorize cloud allocation, uploads, account consent or training costs. Use explicit session identifiers and preserve unrelated runtimes. On interrupted operations inspect remote state before retrying. End with the notebook/script paths, observed hardware, dependency/model/data revisions, completed checks, durable checkpoint location, and whether the runtime is still consuming resources.

Check current official documentation and installed command help when executing: Colab limits, available accelerators, CLI authentication defaults and training APIs change. Never replace missing capabilities with invented endpoints, session cookies or fabricated successful connections.

Python is part of the selected notebook workflow, not a required PocketHarness runtime. A local notebook file alone does not prove that a Colab runtime was reached.
