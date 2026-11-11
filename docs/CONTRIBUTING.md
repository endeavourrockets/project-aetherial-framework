# Contributing

Keep changes small enough for someone else to understand over a cup of tea. If you are changing an interface another board uses, talk to its owner first.

## Branches and commits

Use `dev` to bring work together and `main` for reviewed milestones. Start a branch from `dev`, with a name that says what you are doing: `feat/sensor-parser`, `fix/log-queue`, `docs/setup` or `poc/can-sim` are all fine. No naming ceremony needed. If your work depends on an unmerged branch, mention that in the PR.

Make focused commits with a short explanation of the change. Prefixes such as `feat:`, `fix:`, `test:` and `docs:` are handy, but the useful part is the message. Open a PR into `dev` and ask a teammate to look it over. Pushing a branch does not create the PR for you. Avoid rewriting a branch other people are using.

## Code and checks

- Follow the nearby C style and use names with obvious units where it helps. Comments should explain intent or a surprising limitation.
- Put handwritten board code in `firmware/<board>/App/`. In CubeMX-generated files, use `USER CODE` blocks. Commit `.ioc` changes with the generated files and tell the team about pin/clock/peripheral changes.
- Keep POCs under `poc/`, with their own run commands and a note about what is simulated. Working experiments can be moved into the firmware once reviewed.
- Keep build output and local tool paths out of Git. Shared presets belong in `CMakePresets.json`; personal paths belong in `CMakeUserPresets.json`.
- Build the affected firmware and run the [PC tests](../tests/README.md). Add tests for new calculations, parsing and decisions where they are useful. A bug fix is a good opportunity for a test that catches the original bug.
- Use Renode for relevant firmware checks, and describe what you observed. The current OBC script is a smoke check, not a full hardware model. Say which checks still need a real board; an unrun test is not a passing test.

## Small PR template

Copy this into the PR description and delete anything that does not apply:

```markdown
## What changed and why

## Checks
- Commands run and results:
- Renode or bench observations, if relevant:
- Still untested:

## Notes for the reviewer
Dependencies, hardware assumptions, or anything you would like a second opinion on.
```
