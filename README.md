# EmberSim CI Validation

This repository proves that EmberSim firmware regression testing runs on GitHub Actions hosted runners.

## Structure

```
embersim-ci-validation/
├── .github/workflows/embersim.yml   # Generated CI workflow
├── firmware/                          # Minimal STM32 firmware
│   ├── app.c                         # TIM2 + UART application
│   ├── host_main.c                   # EmberSim host runner
│   └── stm32f4xx_hal.h              # HAL header (fixture)
└── embersim.toml                     # Project configuration
```

## CI Workflow

On every push and pull request to `main`:

1. Checkout repository
2. Install Rust toolchain
3. Install gcc
4. Build EmberSim CLI
5. `embersim check` — validate project readiness
6. `embersim run` — build, execute, compare against baseline

## Setup

1. Clone this repository
2. Set `HAL_HEADER` repository variable: `firmware/stm32f4xx_hal.h`
3. Push — CI runs automatically
