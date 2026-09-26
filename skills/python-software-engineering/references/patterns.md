# Python Software Engineering: patterns and examples

## Package and resource ownership
Use a project environment; do not overwrite Ubuntu's managed interpreter. Keep import-time effects small. Prefer explicit configuration and pathlib for paths; distinguish paths from file contents. Context managers own files, transactions and locks. Structured exceptions carry actionable context without hiding the original cause. Type hints help callers and analysis but do not validate JSON at runtime. Avoid mutable default arguments, broad silent catches and module-global clients with ambiguous lifetimes.

```python
from pathlib import Path
import json

def read_config(path: Path) -> dict:
    with path.open(encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError("configuration must be an object")
    return value
```
Add schema/business validation as needed. Never unpickle untrusted data; YAML loaders and model checkpoint loaders also have distinct trust modes.

## Runtime modes
Backend: use the actual ASGI/WSGI contract; bound request bodies, timeouts and connection pools. Async code must not block the event loop with CPU work or synchronous network calls. Cancellation needs finally/context-manager cleanup and should usually propagate. Threads can help I/O; CPU parallelism depends on interpreter build, native extensions and workload, so benchmark rather than assert a universal GIL rule.

Desktop: keep GUI objects on the toolkit's required thread; workers report via the toolkit's scheduling mechanism. Do not update widgets from a process callback. Linux CLI: use subprocess argument arrays, check return status and distinguish stdout data from stderr diagnostics; avoid shell=True for untrusted values.

ML: fit preprocessing on training data only, record data/model/environment versions, use explicit dtype/device and separate training from inference modes. Avoid accidental GPU synchronization in hot loops. A seed is not a universal reproducibility guarantee across devices, kernels and versions.

## Release and testing
Test installed-package imports, not only execution from the repository root. Use pytest or the existing framework, type/lint checks appropriate to the project, and a small integration test for external boundaries. Preserve tracebacks while redacting secrets. For services, exercise graceful shutdown; for batch jobs, test resumability and atomic output publication.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://docs.python.org/3/
- https://packaging.python.org/en/latest/
- https://docs.python.org/3/library/asyncio-task.html
