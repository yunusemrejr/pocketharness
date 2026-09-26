---
name: research
description: Research technical questions using primary sources, literature searches, evidence grading and reproducible experiments. Use for source-backed comparisons and resolving uncertain claims.
---


# Research

## Frame before you search

Write the question as a **falsifiable, answerable statement**: not "is Rust better?" but "for <this service type>, does Rust reduce p99 latency or memory vs the current Go impl at ≤ 2× dev time?" If you can't state what would count as an answer, the research will produce vibes.

Decide the decision the research feeds (build/buy, A/B, go/no-go) — it sets the depth: a $ decision is not a 20-minute answer.

## Source hierarchy (always)

1. **Primary**: the paper with code/data, the source code, raw benchmarks with repro steps, official specs.
2. **Official docs** of the vendor/project (version-pinned! docs rot — record the version/date you read).
3. **Reproducible secondary**: blog posts that show commands + output, conference talks with repos.
4. **Community**: HN/Reddit/discussions — leads for *where to look* and *who else hit this*, never the proof.
5. **Vendor marketing**: directionality only.

Rule: a claim's strength is the strength of its *weakest* link — a paper claim supported by a vendor blog post is a vendor blog post.

## Literature workflow

- **arXiv**: search with syntax (`cat:cs.LG AND all:"retrieval augmented"`), sort by relevance then date; check the comment field for "code at …"; read abstract → conclusion → methods → experiments (skip the proof first pass).
- **Snowball**: from a good paper — *forward* (who cites it: Google Scholar "cited by", Semantic Scholar API) for the active frontier; *backward* (references) for foundations. Two rounds max per direction; beyond that you're in trivia.
- **Semantic Scholar API** (`api.semanticscholar.org/graph/v1/paper/…`) for programmatic exploration (citations, TLDR, open-access PDF).
- Read 2–3 papers *in parallel* per theme and compare claims directly; contradictions between reputable papers are findings, not noise — note what setting differs (hardware, data scale, year).
- Note discipline: one note = one claim + one source (link + version/date) + one line of "why it matters to our question." No claimless summaries, no sourceless opinions. (A flat list of these > a polished essay you'll never reread.)

## Evidence grading (for any numeric claim)

- **Design**: randomized controlled > controlled > before-after > anecdote. "Self-reported 10×" is anecdote.
- **N and CI**: n=1 benchmarks and "p<0.05" without effect size are not evidence. A 2% improvement with a 40% confidence interval is indistinguishable from zero.
- **Baseline quality**: the classic trick — compare your method to a strawman baseline. Check WHO they beat, not just THAT they beat something.
- **Incentives**: a benchmark from the team that built the tool; check whether the comparison set includes the obvious incumbent (if the incumbent is mysteriously absent, that's the finding).
- **Reproducibility**: is there a script/container? Can *you* run it in an hour? If you can, do — the gap between the paper's number and your number is where the real information lives (hardware, versions, hidden config).

## Experiments (running your own)

- **One variable at a time.** "Change model + prompt + temperature" yields a single non-reproducible miracle.
- **Baseline + control first**: the boring version must exist and be measured, or the interesting version has no meaning.
- **Seeds & repeats**: 3+ seeds or 3+ runs for anything stochastic; report mean AND spread; a result that flips under a seed change is a discovery about variance, not about the method.
- **Ablate the thing you actually claim**: claim "RAG improves this" → ablate: same model, no retrieval. Claim "prompt rewrite improved" → ablate: old prompt, same data.
- **Instrument the cost axis**: tokens/time/money alongside quality — a quality win that 5×'s cost is a tradeoff to report, not a win to claim.
- **Record everything**: exact commands, commit hashes, seeds, data snapshot ids, environment (container/OS/versions), timestamps, raw outputs (not screenshots of charts). If a result can't be regenerated from the record, it didn't happen.

## Triangulating a specific technical claim

Docs → source code → paper → community reports: each layer can correct the layer above. The *source code* is the only tiebreaker for "does it actually work": the docs describe intent, the code describes reality. If the claim is about behavior, read the relevant function, not the README.

## Writing findings

Format (order matters — the reader decides whether to read detail after the top):
1. **Answer** (2–4 sentences, commit to a position: "use X" / "Y is not worth it", with the one-line reason).
2. **Key evidence** (numbered, each tied to a source + your own test where relevant).
3. **What we tested** (method, in compressed form: commands, hardware, data, run counts).
4. **Caveats & failure conditions** (where the answer might not hold — the part that earns trust).
5. **Open questions** (what would flip the answer, and how to find out).

Discipline: label every statement as **fact** (measured, source cited) / **speculation** (plausible, unverified) / **assumption** (taken for granted, state it). No unlabeled.

## Pitfalls

- **Confirmation bias**: search for the disconfirmation first ("X is worse than / fails / slow"), same effort as the pro-side.
- **Recency bias**: the newest paper ≠ the current best; check the citation velocity and the "state of the art" table in the 2 most recent surveys in the field.
- **Vitamin D of benchmarking**: numbers collected to impress rather than to discriminate — if two options both "score 95", the metric has no resolution for your decision; build one that does.
- **Sunk cost framing**: "I spent two days on this" is not evidence; the findings stand or fall on the record.
- **Scope creep**: the question was "which cache library", you're now writing a thesis on memory corruption. Re-read the framed question at every phase boundary; stop when the decision is supportable.

## Detailed coverage

Technical research & evidence work — framing questions, source hierarchy (primary > secondary), literature search (arXiv, citation snowballing), evidence grading, experiment design (controls, seeds, ablations), reproducibility records, triangulating claims, and writing findings. Use for any "is X true / which option wins / what does the literature say" task.

## Recovering web discovery

Use available `web_research` for distinct query angles and known `sourceUrls` in background. Known primary-source URLs can still be read when search engines fail. Select `fallbackProviders` for relevant coverage: Wikipedia locates encyclopedia references; Crossref locates scholarly metadata. Neither substitutes for general web coverage or full-text verification. Honor recency/domain filters and provider cooldowns; do not rotate endpoints to evade access blocks. Start with the default three source reads, or set `readPages:0` for discovery only. Read query attempts and `view:"sources"` receipts; retrieve exact cached passages with `get_search_content` using the source's `responseId`. Keep notes tied to the source and retrieval date, follow useful citations with another bounded source batch, and seek conflicting evidence. Empty results and unreadable pages leave coverage incomplete.
