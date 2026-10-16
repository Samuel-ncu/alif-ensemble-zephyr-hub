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
├── docs/
│   └── HARDWARE_GPIO_REFERENCE.md # Verified DevKit Pinout, LEDs, SW2, & DesignWare rules
├── examples/
│   ├── 01_hello_world/          # Verified Example 01: UART Console Output
│   ├── 02_blinky/               # Verified Example 02: GPIO LED1_G 1Hz Toggle
│   ├── 03_button/               # Verified Example 03: SW2 5-Way Joystick Interrupt
│   └── 04_hello_rtt/            # Example 04: SEGGER RTT Real-Time Logging (Zero SW4 Switch)
├── scripts/
│   └── app-write-mram           # Automated serial unlocking flash wrapper
├── LICENSE                      # Apache License 2.0
├── AGENTS.md                    # Agent instructions & guidelines (this file)
└── README.md                    # Project overview & documentation
```

## How AI Agents Should Use This Repository

1. **Skill Discovery & Hardware Truth**:
   - Inspect `.agents/skills/<skill-name>/SKILL.md` before executing build/flash commands.
   - For all GPIO, LED, switch, and peripheral mappings, strictly consult [`docs/HARDWARE_GPIO_REFERENCE.md`](docs/HARDWARE_GPIO_REFERENCE.md).
   - **Crucial Driver Rule**: DesignWare GPIO (`gpio_dw.c`) forbids `GPIO_PULL_UP`/`GPIO_PULL_DOWN` software flags. Use `GPIO_INPUT` only.

2. **Skill Activation**:
   - Always read the relevant `SKILL.md` before attempting multi-step build, flash, or hardware tasks.
   - Follow the prescribed hardware switch states, wrapper patches, and port assignments strictly.

3. **Adding New Skills**:
   - When creating a new skill, create a dedicated folder under `.agents/skills/<skill-name>/`.
   - Include a valid `SKILL.md` with standard YAML frontmatter (`name`, `description`).
   - Structure procedures logically: Hardware requirements, Environment setup, Toolchain, Build, Flash, Verify, and Troubleshooting Checklist.

