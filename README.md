Digital USB GPIO / programmable relay concept and source code

## Overview

The goal is to create hardware device which acts like programmable relay. It has 6 independently configurable digital GPIO ports, on-board non-volatile memory (EEPROM) and is capable of doing its job without USB connection.

Each output pin has a formula which determines its value. Formulas can contain any of 6 IO ports, 4 variables and NOT, AND, OR, XOR operators. Formulas, as well as variables, can be programmed via USB serial interface.

The solution is based on Arduino Pro Micro. It should work with any other Arduino as well, though latency might become a problem with board without native USB support. In future, there might be an implementation which uses USB interrupts for low-latency messages.

There will be a webpage to graphically interract with device via WebUSB.

## Protocol

Data primarily moves from host to device. Device accepts following commands:

| Command            | Binary     | Semantics |
|--------------------|------------|-----------|
| Configure pins     | `00xxxxxx` | `x` bits determine if pin is output (`1`) or input (`0`) |
| Set formula        | `01xxxxxx` | `x` bits determine the subject pins (outputs) of the formula. After sending this byte, a formula should follow as described below. There is timeout of 200ms during which the formula must be transmitted. |
| Read pins          | `11000000` | after this command, device sends `11xxxxxx` to host where `x` bits determine the state of all GPIO pins (incl. output) |
| Set trigger        | `1101xyyy` | `x` determines whether trigger will be set (`1`) or unset (`0`). `yyy` determines the ID of the pin. |
| Set variables      | `1110xxxx` | `x` determines the value of corresponding variable. |
| Set variable       | `11110xyy` | `x` determines the value of the variable, `yy` ID of the variable |
| N/A                | `11111xxx`, `10xxxxxx` | no op, available for future use |
| Save configuration | `11111111` | save configuration, formulas and variables to EEPROM |

### Formula

Formulas are transmitted and stored in RPN format. Each element (operand or operator) is presented by 1 byte.

Individual elements have following meaning:

| Value  | Meaning       |
|--------|---------------|
|  `00`  | pin 0         |
|  `01`  | pin 1         |
|  `02`  | pin 2         |
|  `03`  | pin 3         |
|  `04`  | pin 4         |
|  `05`  | pin 5         |
|  `06`  | var 0         |
|  `07`  | var 1         |
|  `08`  | var 2         |
|  `09`  | var 3         |
|  `0A`  | NOT           |
|  `0B`  | AND           |
|  `0C`  | OR            |
|  `0D`  | XOR           |
|  `0E`  | N/A (ignored) |
|  `0F`  | terminator    |

Pin operands can also have 5th bit set to pull the value up in case the pin is configured as output. Otherwise, output pins have value `0`.

During evaluation, there will always be at least 1 value in stack - initially zero. Formulas which cause underflow (using binary operator with only 1 element in stack) will be ignored (not assigned to I/O ports). If formula leaves more than one element to stack, the top one would be used.

Output pins could also be used as operands. Formulas set for input pins are saved but ignored until pins get reconfigured as outputs.

### Examples

Set output pins 0 and 4 to constant 1:

 * `0x51` (`01010001` - configure formula for pins 0 and 4)
 * `0x0A` (NOT - turns initial 0 value to 1)
 * `0x0F` (terminator)

Set output pin 3 to be 1 if and only if input pin 5 and variable 2 are equal:

 * `0x59` (`01001000` - configure formula for pin 3)
 * `0x05` (pin 5)
 * `0x08` (var 2)
 * `0x0D` (XOR)
 * `0x0A` (NOT)
 * `0x0F` (terminator)

Set output pins 2 to be equal to pin 1 or `1` if pin 1 is configured as output:

 * `0x51` (`01000100` - configure formula for pins 0 and 4)
 * `0x11` (pin 1 with pullup)
 * `0x0F` (terminator)

## Licence

MIT
