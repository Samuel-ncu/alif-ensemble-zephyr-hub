# AI Agent & Assistant Guidelines

This repository serves as a knowledge and execution hub for AI Coding Agents (such as Google Antigravity, Claude Code, Cursor, Windsurf, and GitHub Copilot).

## Repository Architecture

```text
.
├── .agents/
│   └── skills/                  # Modular AI Skills & Runbooks
│       └── alif-ensemble-zephyr/
│           └── SKILL.md         # Alif Ensemble E8/E7 Zephyr RTOS complete runbook
├── .github/
│   └── FUNDING.yml              # Sponsor and donation configuration
├── examples/
│   ├── 01_hello_world/          # Verified Example 01: UART Console Output
│   ├── 02_blinky/               # Verified Example 02: GPIO LED1_G 1Hz Toggle
│   └── 03_button/               # Example 03: Button SW0 Interrupt & LED Control
├── LICENSE                      # Apache License 2.0
├── AGENTS.md                    # Agent instructions & guidelines (this file)
└── README.md                    # Project overview & documentation
```

## How AI Agents Should Use This Repository

1. **Skill Discovery**:
   - When tasked with configuring, building, flashing, or debugging target hardware or frameworks, inspect the `.agents/skills/<skill-name>/SKILL.md` files.
   - Each skill provides battle-tested workflows, hardware switch configurations, toolchain installations, and troubleshooting procedures.

2. **Skill Activation**:
   - Always read the relevant `SKILL.md` before attempting multi-step build, flash, or hardware tasks.
   - Follow the prescribed hardware switch states, wrapper patches, and port assignments strictly.

3. **Adding New Skills**:
   - When creating a new skill, create a dedicated folder under `.agents/skills/<skill-name>/`.
   - Include a valid `SKILL.md` with standard YAML frontmatter (`name`, `description`).
   - Structure procedures logically: Hardware requirements, Environment setup, Toolchain, Build, Flash, Verify, and Troubleshooting Checklist.

