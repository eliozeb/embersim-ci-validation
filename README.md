# EmberSim CI Validation

Reference customer project — proves a standalone firmware repository can run
EmberSim regression testing on GitHub Actions without physical hardware.

## Structure

```
embersim-ci-validation/
├── .github/workflows/embersim.yml   # Generated CI workflow
├── .embersim/baseline/trace.jsonl   # Golden regression baseline
├── embersim.toml                     # Project configuration
├── firmware/
│   ├── app.c                         # TIM2 + UART application
│   ├── host_main.c                   # EmberSim host runner
│   └── stm32f4xx_hal.h              # HAL header fixture
└── README.md
```

`ember_sim/` is a generated workspace (created by `embersim init`). It is
excluded from version control — CI regenerates it on every run.

## How It Works

1. **Checkout** — CI checks out this repository and EmberSim (`eliozeb/embersim`)
   into `.embersim/`.
2. **Build** — `cargo build --manifest-path .embersim/Cargo.toml` compiles the CLI.
3. **Init** — `embersim init` generates the `ember_sim/` workspace from the HAL header.
4. **Check** — `embersim check` validates HAL coverage.
5. **Run** — `embersim run` builds firmware, executes on the simulator, and compares
   the trace against the committed golden baseline.

The same trace is produced every time — deterministic execution without hardware.

## Setup (first-time)

### 1. Generate the CI workflow

```bash
embersim ci-init
```

This creates `.github/workflows/embersim.yml`.

### 2. Create the golden baseline

```bash
embersim init -f firmware/stm32f4xx_hal.h -I firmware -o ember_sim
embersim run -o ember_sim
embersim baseline create -t trace.jsonl
```

This populates `.embersim/baseline/trace.jsonl`. Commit it.

### 3. Configure GitHub repository variables

Set one **required** variable in your repository:

| Variable | Value |
|----------|-------|
| `HAL_HEADER` | `firmware/stm32f4xx_hal.h` |

GitHub → Settings → Secrets and variables → Actions → Variables

**Optional:** To use a fork of EmberSim, also set:

| Variable | Value |
|----------|-------|
| `EMBERSIM_REPO` | `your-org/embersim` |

If not set, the workflow defaults to `eliozeb/embersim`.

### 4. Push

The workflow triggers on every push and pull request to `main` or `master`.

## CI Workflow Steps

| Step | What it does |
|------|-------------|
| Checkout repository | Checks out this firmware repo |
| Install Rust toolchain | `dtolnay/rust-toolchain@stable` |
| Install gcc | Host compiler for firmware |
| Checkout EmberSim | `actions/checkout@v4` into `.embersim/` |
| Cache Rust dependencies | `Swatinem/rust-cache@v2` scoped to `.embersim` |
| Build EmberSim CLI | `cargo build --manifest-path .embersim/Cargo.toml` |
| Initialize workspace | `embersim init` generates `ember_sim/` |
| Check project readiness | `embersim check` validates HAL coverage |
| Run firmware regression | `embersim run` builds, executes, compares baseline |

## Local Development

```bash
# After cloning, regenerate the workspace
embersim init -f firmware/stm32f4xx_hal.h -I firmware -o ember_sim

# Check HAL coverage
embersim check -o ember_sim

# Build and run (compares against committed baseline)
embersim run -o ember_sim

# Update the baseline after intentional firmware changes
embersim baseline create -t trace.jsonl
```

## Evidence

This repository demonstrates:

- A standalone firmware repo can adopt EmberSim without containing EmberSim source
- CI acquires EmberSim via `actions/checkout` and builds with `--manifest-path`
- `EMBERSIM_REPO` defaults to `eliozeb/embersim` — zero configuration for the happy path
- Deterministic trace output enables regression detection on every push
- Zero kernel, scheduler, or peripheral changes required
