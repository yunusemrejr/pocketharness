# Connections and identity

Reference check: 2026-09-09. Commands below describe supported surfaces; check the installed release before executing them.

## Hosted browser

Open Colab, select the intended Google account, upload/open the `.ipynb`, choose Runtime → Change runtime type and the intended accelerator, then Connect. Confirm the actual device from a code cell: selecting GPU does not place tensors on it. Obtain necessary user sign-in/consent through the browser's normal interface. Reuse an existing authorized session when appropriate. Saving the notebook does not save its VM filesystem. [Colab FAQ](https://research.google.com/colaboratory/faq.html)

With browser tools, inspect the actual controls and execution result rather than assuming selectors or a connected state. Without an execution-capable surface, supply the validated notebook and explicit next action; do not claim training ran. A saved GitHub/Drive notebook URL identifies a document, not exclusive ownership of its current runtime.

## Official CLI and MCP

Google publishes `google-colab-cli`; check `colab version`, `colab --help` and subcommand help. If installation is in scope, use an isolated tool environment with a recorded package version. CLI session commands include `new`, `sessions`, `status`, `exec`, `upload`, `download`, and `stop`. Always target a named session when several may exist. [Official CLI](https://github.com/googlecolab/google-colab-cli)

Example command sequence after verifying current help and authorized hardware:

```bash
colab new -s tune-demo --gpu T4
colab status -s tune-demo
colab install -s tune-demo -r requirements.lock.txt
colab upload -s tune-demo train.py /content/train.py
colab exec -s tune-demo -f preflight.py
colab exec -s tune-demo -f train.py
colab download -s tune-demo /content/run-artifacts/export.tar ./export.tar
colab stop -s tune-demo
```

This is a sequence, not a failure-safe shell script: inspect each result, verify exported files before releasing the VM, and record the session if recovery is needed. The script's other imports, configs and datasets must also be staged; sending `train.py` does not upload its directory. Do not repeat `new` blindly after a timeout.

CLI backend authentication and authentication *inside* the remote kernel are distinct. Upstream README and design notes currently disagree about the default auth provider; inspect installed help/source, select the intended supported provider explicitly, and complete its browser consent flow. The documented providers are OAuth2 and ADC. `colab auth -s tune-demo` authenticates the remote runtime for Google Cloud services; it is not a universal login command. Treat CLI token/session/history files as credential-bearing. [Authentication design](https://github.com/googlecolab/google-colab-cli/blob/main/docs/04_automation_and_utility.md)

`colab run` may release the VM after execution. Inspect that release's artifact-retrieval behavior and arrange durable output inside the job before relying on ephemeral execution; a checkpoint left only on the VM can disappear. A retained session needs explicit cleanup. [Job lifecycle](https://github.com/googlecolab/google-colab-cli/blob/main/docs/05_run_command.md)

An official [Colab MCP server](https://github.com/googlecolab/colab-mcp) is another possible integration. Discover whether it is actually installed and callable; inspect its tools and account scope. A repository's existence does not mean this agent has that connection. Avoid unofficial remote-control workarounds or resource-limit evasion when supported connection is unavailable.

## Local runtime

Use local mode for user-controlled compute. It grants notebook code access to that runtime's files and processes. Bind only loopback; keep the token and exact Colab origin check. Google's Docker runtime is Linux/amd64 and its documentation warns that the supplied images are for demos, potentially containing outdated dependencies. For maintained workloads prefer a reviewed, pinned environment. [Local-runtime instructions](https://research.google.com/colaboratory/local-runtimes.html)

```bash
docker run --gpus=all -p 127.0.0.1:9000:8080 us-docker.pkg.dev/colab-images/public/runtime
```

GPU use needs compatible NVIDIA drivers and container tooling. For direct Jupyter, Google's documented invocation is:

```bash
jupyter notebook --NotebookApp.allow_origin='https://colab.research.google.com' --port=8888 --NotebookApp.port_retries=0 --NotebookApp.allow_credentials=True
```

Verify effective flags for the installed Jupyter version, loopback listening and active token authentication. In Colab's connection menu select local runtime and enter the printed token URL privately. Remote hosts need an authorized secure tunnel, not public unauthenticated Jupyter. Mount only intended working directories into containers. Do not assume `drive.mount()` works in local mode.

## Secrets, Drive and GCS

Hosted UI secrets can be read without embedding a token in the notebook:

```python
from google.colab import userdata
hf_token = userdata.get("HF_TOKEN")
# Pass directly to the authorized library call; never print it.
```

The secret must exist and grant this notebook access. Handle missing secret, access-denied and timeout separately. `userdata` depends on the Colab UI and may fail headlessly; use the execution mode's supported secret delivery instead of endlessly retrying. Clear notebook outputs before sharing; do not persist secrets in run manifests or shell command strings. [Official userdata implementation](https://github.com/googlecolab/colabtools/blob/main/google/colab/userdata.py)

For hosted Drive access, use `from google.colab import drive; drive.mount('/content/drive')` and complete the intended account consent. Drive is suitable for durable archives, not high-frequency small-file training I/O. CLI mode has `colab drivemount -s SESSION`; verify that release supports the authentication flow.

For GCS in a hosted notebook, authenticate the user with `google.colab.auth.authenticate_user()`, then use `google.cloud.storage.Client(project=PROJECT_ID)` with explicit project and bucket. ADC supplies credentials; IAM still controls access. Listing a bucket is not proof of write permission. Use a dedicated run prefix and verify a small round trip when authorized. Prefer scoped runtime identities or federation to uploaded service-account keys. [Cloud Storage authentication](https://docs.cloud.google.com/storage/docs/authentication)

## Enterprise

Colab Enterprise uses Google Cloud project, region, billing, IAM and runtime templates. Connect to the intended existing runtime; starting one may allocate billed compute. End-user credentials and runtime service identities differ. Do not apply consumer subscription assumptions or Drive mounting instructions to Enterprise. Use the current project documentation for required APIs and roles rather than granting broad administrator access to fix an unspecified permission error. [Enterprise runtime connections](https://docs.cloud.google.com/colab/docs/connect-to-runtime)
