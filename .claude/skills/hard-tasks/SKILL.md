---
name: hard-tasks
description: Working method for hard, multi-step, or not-yet-understood problems — how to decompose them into verifiable steps, verify results against primary evidence instead of assumptions, and pick the next action under uncertainty. Use when debugging unknown failures, doing multi-stage integrations, or making changes where being wrong is expensive.
---

# Hard tasks: decompose, verify, decide

Operating rules for work where the path is not fully known. The unifying rule:
**prefer evidence over inference, and inference over memory.** Everything below
is that rule applied at three moments — planning, checking, and choosing.

## 1. Decompose

- **Split at observable boundaries, not conceptual ones.** A subtask is done
  when something checkable exists — it builds, a log line appears, `git status`
  is clean, bytes on disk changed. "Code written" is not a boundary.
- **Sequence by what unblocks verification earliest.** Wire init/update/draw
  before polish; make it fail loudly before making it correct; make it correct
  before making it good.
- **Separate knowledge from assumption at the start.** State explicitly what
  you have confirmed vs. what you believe. Assumptions are debts — name them
  to the user and schedule their verification; don't let them silently become
  facts.
- **Park what isn't yours to decide.** Design taste, tooling choices, asset
  policy, anything irreversible — surface it, mark it deferred, and continue
  with the parts that are safe. Never resolve a user decision by default.
- **One logical change per commit.** Code, assets, docs, and diagnostic
  scaffolding are separate commits. This is not tidiness: history is your
  restore mechanism (deleted files can be resurrected, "before" states can be
  diffed) and your forensics tool.

## 2. Verify

- **Evidence hierarchy.** Bytes on disk > command output > runtime logs > your
  own earlier statements > plausible theory. When layers disagree, the lower
  one wins. Hex-dump the header instead of trusting the export dialog; grep the
  compiled binary instead of trusting documentation; `ls` the folder instead of
  trusting the plan.
- **Reproduce before theorizing.** An error dialog is not a reproduction; the
  log line immediately before it is. Get the actual failing output in hand
  before proposing causes.
- **If you can't observe, instrument.** Add temporary logging with one uniform
  tag so removal is a single grep. Remove it in its own commit once the cause
  is confirmed. Never let scaffolding blend into product code.
- **Diff against the nearest working sibling.** The fastest diagnosis of a
  broken thing is a working thing of the same kind: same file format, same
  code path, same config. Compare them mechanically, change one variable per
  experiment.
- **Re-audit your own claims with primary evidence, not by re-reading your
  reasoning.** When the user pushes back, or when a "solved" problem persists,
  go back to the artifacts and re-derive. Expect to find your own error. When
  you find one: say so plainly, identify every conclusion built on it, and
  invalidate those too.
- **Verify the environment, not just the code.** Stale caches, duplicate
  copies of assets, wrong working directory, parallel edits by humans,
  untracked files. Check shared state (`git status` on the main tree) every
  single time before integrating — especially when you're sure it's clean.
- **"Done" is an enumerated checklist.** Clean status, expected bytes at the
  expected version, zero leftover debug code, docs updated. Check each; don't
  round up from "the main thing works."

## 3. Decide what's next

- **Run the cheapest discriminating experiment.** Pick the action whose result
  splits the surviving hypotheses (swap in the known-good file; lower one
  version; toggle one node). If a test's outcome wouldn't change what you do,
  skip it.
- **When two explanations remain, don't argue — design the split.** A/B tests
  beat rhetoric: "if the sibling also fails, it's the runtime; if not, it's
  the file."
- **Ask when the answer changes the work; act when it doesn't.** If every
  reasonable answer leads to the same next step, proceed and note the choice.
  If the user's answer would change what you build, delete, or publish — stop
  and ask, with a recommendation attached.
- **If the same probe fails twice, change instruments, not effort.** A tool
  that produced nothing twice (a debugger window, a search, a log filter)
  will not produce something the third time. Switch observation method
  entirely.
- **When evidence contradicts the plan, stop the plan.** Do not push through
  on momentum. Re-verify the premises the plan was built on; one wrong
  observation upstream invalidates everything after it.
- **Escalate observability stepwise.** Read existing output → inspect state →
  add instrumentation → analyze formats/binaries. Don't reach for heavy
  tooling before cheap reading has failed, but don't stall on guesswork when
  a probe would settle it.
- **Close loops at the end.** Remove scaffolding, record the durable lesson
  where the next session will find it (project docs, memory), and state
  faithfully what was wrong, what fixed it, and which of your own claims
  didn't survive.

## Anti-patterns

Each of these has caused real damage:

- Trusting your memory of a file over the file.
- Building a diagnosis on an observation without checking the observation was
  current.
- Deleting "redundant" artifacts before confirming nothing needs them.
- Instrumentation left behind, unmarked, after the mystery is solved.
- Integrating into shared state without checking what humans changed there.
- Reporting "fixed" when only the visible symptom was checked.
