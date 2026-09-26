# Go Service Engineering: patterns and examples

## Service contract
Accept `context.Context` as the first argument where cancellation applies; propagate it to network/database calls. Do not store arbitrary request contexts in long-lived structs. A timeout stops waiting only if the callee observes cancellation; it does not undo a remote mutation.

```go
func fetch(ctx context.Context, client *http.Client, url string) error {
    req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
    if err != nil { return err }
    resp, err := client.Do(req)
    if err != nil { return err }
    defer resp.Body.Close()
    if resp.StatusCode != http.StatusOK { return fmt.Errorf("unexpected HTTP status %d", resp.StatusCode) }
    _, err = io.Copy(io.Discard, io.LimitReader(resp.Body, 1<<20))
    return err
}
```
This sketch discards at most 1 MiB; it does not prove the entire body was read or reusable connection draining. Real parsing should detect oversize payloads, validate schema and use configured client deadlines. Check URLs at the trust boundary to prevent SSRF when user-controlled.

## Concurrency and state
A worker pool should select on cancellation both when receiving and sending. The sender that owns completion closes a channel; multiple workers must not race to close it. Bound queues and define rejection/backpressure rather than creating a goroutine per unbounded item. Avoid copying types containing mutexes. Slice views may alias an unexpectedly large backing array; nil interfaces can contain non-nil typed pointers.

## Operations
Wrap errors with `%w` where callers need errors.Is/As. Graceful shutdown stops admission, drains bounded in-flight requests and eventually terminates. Use race tests for concurrent code, table tests for edge cases and benchmarks with allocation reporting. Race detection covers executed schedules only. Check cgo/native libraries and certificate/timezone assets before claiming a static container is self-contained. Compare build tags and architecture-specific files on intended Linux targets.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://go.dev/doc/
- https://go.dev/doc/effective_go
- https://pkg.go.dev/net/http
