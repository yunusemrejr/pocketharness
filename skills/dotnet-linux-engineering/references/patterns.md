# C# and .NET on Linux: patterns and examples

## Contracts and async lifetime
Enable the project's nullable analysis and validate external data at runtime. A non-null annotation is not JSON validation. Use `using`/`await using` for deterministic cleanup and pass cancellation through owned asynchronous operations.

```csharp
static async Task<string> ReadTextAsync(string path, CancellationToken ct)
{
    return await File.ReadAllTextAsync(path, ct);
}
```
This loads the entire file; impose input limits or stream large/untrusted files. A canceled operation may have completed a remote side effect, so retries need a reconciliation contract. Avoid async void outside event-handler boundaries and avoid sync-over-async blocking. Reuse appropriately managed HTTP clients; set deadlines and response size limits.

## Linux deployment
Path casing, separators, file permissions, signals, locale and certificate stores differ from a Windows development machine. Check ICU/globalization and native library dependencies. WinForms/WPF and Windows-specific APIs are not a generic Linux desktop solution; choose a supported UI stack and test input/rendering on the actual display system. Do not infer GUI support from successful compilation.

Framework-dependent and self-contained deployments trade installed runtime requirements against artifact size and update responsibility. Trimming and Native AOT can break reflection-heavy serializers, dynamic loading and some libraries; run the actual published output and inspect warnings. Runtime identifiers and libc compatibility matter for native assets and containers.

## Performance and service behavior
Keep request admission, hosted worker shutdown and dependency disposal ordered. Bound Channels/queues and define backpressure. Span/Memory views require valid lifetimes; pooling demands one return owner and no access after return. Use decimal for domain-required decimal arithmetic, and deliberate double/vector types for numerical work. Benchmark allocations and latency with representative payloads; JIT warmup and GC can dominate tiny measurements. Tests should include cancellation, case-sensitive paths, invalid input and graceful termination, not only happy-path unit calls.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://learn.microsoft.com/en-us/dotnet/core/install/linux
- https://learn.microsoft.com/en-us/dotnet/csharp/
- https://learn.microsoft.com/en-us/dotnet/core/deploying/
