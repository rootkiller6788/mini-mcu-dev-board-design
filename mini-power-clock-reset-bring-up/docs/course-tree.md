# Course Dependency Tree - mini-power-clock-reset-bring-up

## Prerequisites
1. Circuit Analysis (Ohm, KCL, KVL, RC/RL) -> power, reset, PDN
2. Analog Electronics (transistor, op-amp) -> LDO, Pierce, comparator
3. Digital Electronics (logic, timing) -> reset seq, debounce, WDG
4. Signal Processing (Fourier, Laplace) -> PLL, phase noise, jitter
5. EM Theory (Maxwell, TL) -> PDN impedance, microstrip, EMC

## Module Dependency
[Circuit Analysis] --> [Power Design] --> [Clock Design] --> [Reset Design] --> [Bring-Up + Validation]

## Skill Progression
1. power_design.h/c (foundational)
2. clock_design.h/c (builds on power)
3. reset_design.h/c (builds on both)
4. bring_up.h/c + board_validation.h/c (integration)
