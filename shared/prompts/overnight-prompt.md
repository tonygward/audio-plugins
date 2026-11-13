# Matrix overnight execution

Matrix runs this process itself; nothing here needs to be pasted into an agent. It is kept as the entry point for anyone running a task by hand.

1. **Pick one card** as the [execution policy](../../docs/songsmith-execution-policy.md#2-selection-what-runs-next) says: Doing first, then the top of Todo, in board order. Backlog and Blocked cards wait.
2. **Implement it** from its prompt (or its issue, when there is no prompt yet) following [CLAUDE.md](../../CLAUDE.md), on a branch from current `main`, within the [limits](../../docs/songsmith-execution-policy.md#3-bounded-execution). Sonnet is the default model.
3. **Open a pull request.** It merges automatically once the exact head commit passes CI; the version bump, packaging build, release and website follow on their own ([pipelines](../../docs/pipelines.md)).
4. **Finish** as the policy's [Finishing a task](../../docs/songsmith-execution-policy.md#8-finishing-a-task) says: final comment with the download link, close the issue, move the card to Done.
