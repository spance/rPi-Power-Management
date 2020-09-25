# Introduction

For a long time, we have controlled the power state of the Raspberry Pi by plugging and unplugging the power supply. Turning on the power will cause the system to start, but shutdown the system does not stop the power supply. This reminds me of before 2000, the 586 was popular, after the shutdown command was executed, waiting for the screen prompt and then pressing the switch to turn off the power, this posture is really not graceful!

I have some confusion about the power management of the Raspberry Pi:

- If the system shutdown without power down, **continuous light pollution, continuous wind noise, continuous power consumption**, too bad!
- Even if the power is cut off by unplugging power supply, after executing `shutdown` command, wait **how many seconds** before it can be safely cut off?
- Why does the power-off operation require **human manual execution? **

In essence, lack of supporting `ACPI` is the major issue in Raspberry Pi.

# Solution

So I decided to design a low-cost custom hardware solution for the Raspberry Pi to solve this trouble, I will share my solution below.

## Goals

- No need to run any programs or triggers in the Raspberry Pi
- Minimal physical wires with Raspberry Pi
- Refer to ATX/ACPI power management features
- Based on low-cost hardware facilities and easy to assemble
- Long-term, low power consumption, highly reliable operation

Any Power management cannot meet the above requirements is a rogue. Each of the above is strictly abided by this solution, which is also the advantage of this solution.

## Features

1. Press the button to startup gracefully, instead of turning power supply to startup.
2. When the Raspberry Pi shuts down, it will automatically power down after a delay, without the need for humans.
3. During the power on period, long press the button to force the power down.
4. During the delayed power down, press the button to cancel the delayed power down.

This is the **implemented features**, I will consider upgrading the software and hardware solutions to add more features in the future.

## Schematic

![schematic](schematic.png)

## Required modules

- High-power DC-DC conversion module (such as LM2596 XL4005 XL6009 MP1584, etc.)
  - If the power supply is higher than 5V, it is a step-down module
  - No need if it is a direct 5V power supply
  - If the power supply is lower than 5V, it is a boost module (such as 3.7V lithium battery)
- Arduino Pro Mini (5V, 16MHz)
- LR7843 MosFET (or other high-power models, Mos module is recommended)
- Buttons, plugs, wires, etc.

Note: The total cost should be less than $3.

## State machine

![state-machine](state-machine.png)

## Triggering conditions

The core of this power management is state transition, and choosing a reliable and easy-to-implement conditional edge is the core factor for the success of the solution:

### Condition of power on

- [x] button trigger
- [ ] *Timed trigger (TODO1)*

### Condition of power off

- [x] button trigger
- [x] Kernel log of the serial terminal
- [ ] *Power supply current reduction (TODO2)*

Note: For TODO1 RTC clock needs to be added, and for TODO2 Hall sensor needs to be added.

# How to install

1. Install the circuit module according to the schematic diagram, pay attention to the pin connections
2. Write the `rPiPwrMngt.ino` program provided by this repository to `Arduino`
3. Enable the Raspberry Pi serial port `serial` via `raspi-config`

If you are skilled, you should be able to complete the installation in 1-2 hours.

# Custom installation

If you are not connecting according to the pins in the figure, please modify the definitions according to the actual situation:

```c
// the pin number of MosFET module
const int PIN_RELAY = 7;
// the pin number of power button
const int PIN_POWER_BTN = 4;
```
