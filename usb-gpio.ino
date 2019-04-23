#include <WebUSB.h>

const int PIN_COUNT = 6;
const int VAR_COUNT = 4;
const int PINS[] = { 3, 2, 0, 1, 4, 12 };
const int FORMULA_TIMEOUT = 200;
const int MAX_FORMULA_SIZE = 64; // including end byte (0f)

const int OP_NOT = 0x0a;
const int OP_AND = 0x0b;
const int OP_OR = 0x0c;
const int OP_XOR = 0x0d;

uint8_t outputs = 0;
uint16_t state = 0; // 0 is low
uint8_t serialTriggers = 0; // 0 is unset
uint8_t serialWebTriggers = 0; // 0 is unset

uint8_t formulas[MAX_FORMULA_SIZE][PIN_COUNT] = { {0}, {0}, {0}, {0}, {0}, {0} };
uint8_t dependents[PIN_COUNT + VAR_COUNT] = { 0 };

WebUSB SerialWeb(1 /* https */, "keijo.ee/webusb-test");

void setup() {
	Serial.begin(19200);
	SerialWeb.begin(0);
}


/**
 * commands
 */

void cmdConfiguration(uint8_t configuration) {
	outputs = configuration & 0x3f;
	for(int i = 0; i < PIN_COUNT; i++) {
		pinMode(PINS[i], outputs & 1 << i ? OUTPUT : INPUT);
		if(outputs & 1 << i) {
			bool value = calculate(formulas[i]);
			bool changed = value != bool(state & 1 << i);
			if(value) {
				state |= 1 << i;
			} else {
				state &= ~(1 << i);
			}
			if(changed) {
				digitalWrite(PINS[i], value);
				trigger(i);
			}
		}
	}

	flush();
}

void cmdOutput(Stream& stream, uint8_t pins) {
	unsigned long start = millis();
	uint8_t formula[MAX_FORMULA_SIZE];
	int length = 0;
	uint16_t dependencies = 0;
	int stackCounter = 1;

	pins &= 0x3f;

	while(length < MAX_FORMULA_SIZE && millis() - start < FORMULA_TIMEOUT) {
		if(stream.available()) {
			int c = stream.read();

			if(c < 0) {
				continue;
			}

			if(c == 0x0f) {
				formula[length++] = c;
				if(stackCounter >= 1) {
					bool value = calculate(formula);
					uint8_t newOutputState = 0;
					for(int i = 0; i < PIN_COUNT + VAR_COUNT; i++) {
						if(pins & 1 << i) {
							memcpy(formulas[i], formula, length);
							newOutputState |= value << i;
						}

						dependents[i] &= ~pins;
						if(dependencies & 1 << i) {
							dependents[i] |= pins;
						}
					}

					uint8_t changedOutputs = (state ^ newOutputState) & pins;
					state &= ~pins;
					state |= newOutputState;

					for(int i = 0; i < PIN_COUNT; i++) {
						if(changedOutputs & 1 << i) {
							digitalWrite(PINS[i], value);
							trigger(i);
						}
					}

					flush();
				}
				return;
			} else if((c & ~0x10) < 0x0a) {
				stackCounter++;
				dependencies |= 1 << (c & 0x0f);
				formula[length++] = c;
			} else if(c < 0x0e) {
				if(c != OP_NOT) {
					stackCounter--;
				}
				formula[length++] = c;
			}
		}
	}
}

void cmdInput(Stream& stream) {
	stream.write((uint8_t)(state & ~outputs) | 0xc0);
	stream.flush();
}

void cmdTrigger(Stream& stream, uint8_t arg) {
	uint8_t& triggers = &stream == &SerialWeb ? serialWebTriggers : serialTriggers;
	int pin = arg & 0x07;
	if(pin < PIN_COUNT) {
		if(arg & 0x08) {
			triggers |= 1 << pin;
		} else {
			triggers &= ~(1 << pin);
		}
	}
}

void cmdVariables(uint8_t arg) {
	uint16_t vars = arg & 0x0f;

	uint8_t affectedOutputs = 0x00;
	for(int i = 0; i < VAR_COUNT; i++) {
		if(bool(state & 1 << (i + PIN_COUNT)) != bool(vars & 1 << i)) {
			affectedOutputs |= dependents[i + PIN_COUNT];
		}
	}

	state ^= (-vars ^ state) & 0x03c0;

	for(int i = 0; i < PIN_COUNT; i++) {
		if((outputs & 1 << i) && (affectedOutputs & 1 << i)) {
			bool value = calculate(formulas[i]);
			if(value != bool(state & 1 << i)) {
				if(value) {
					state |= 1 << i;
				} else {
					state &= ~(1 << i);
				}
				digitalWrite(PINS[i], value);
				trigger(i);
			}
		}
	}

	flush();
}

void cmdVariable(uint8_t arg) {
	int var = arg & 0x03;

	uint8_t affectedOutputs = dependents[var + PIN_COUNT];

	if(arg & 0x04) {
		state |= 0x40 << var;
	} else {
		state &= ~(0x40 << var);
	}

	for(int i = 0; i < PIN_COUNT; i++) {
		if((outputs & 1 << i) && (affectedOutputs & 1 << i)) {
			bool value = calculate(formulas[i]);
			if(value != bool(state & 1 << i)) {
				if(value) {
					state |= 1 << i;
				} else {
					state &= ~(1 << i);
				}
				digitalWrite(PINS[i], value);
				trigger(i);
			}
		}
	}

	flush();
}

void cmdSave() {
	// TODO
}

/**
 * loop helpers
 */

void processStream(Stream& stream) {
	if(stream.available()) {
		int c = stream.read();
		if(c == 0xff) { // 8 bit command
			cmdSave();
		} else if((c & 0xf8) == 0xf0) { // 5 bit command
Serial.println("Variable: " + String(state, 2));
			cmdVariable(c);
Serial.println("Done: " + String(state, 2));
		} else if((c & 0xf0) == 0xe0) { // 4 bit command
Serial.println("Variables: " + String(state, 2));
			cmdVariables(c);
Serial.println("Done: " + String(state, 2));
		} else if((c & 0xf0) == 0xd0) { // 4 bit command
Serial.println("Trigger: " + String(state, 2));
			cmdTrigger(stream, c);
Serial.println("Done: " + String(state, 2));
		} else if((c & 0xc0) == 0xc0) { // 2 bit command
Serial.println("Input: " + String(state, 2));
			cmdInput(stream);
Serial.println("Done: " + String(state, 2));
		} else if((c & 0xc0) == 0x40) { // 2 bit command
Serial.println("Output: " + String(state, 2));
			cmdOutput(stream, c);
Serial.println("Done: " + String(state, 2));
		} else if((c & 0xc0) == 0x00) { // 2 bit command
Serial.println("Configuration: " + String(state, 2));
			cmdConfiguration(c);
Serial.println("Done: " + String(state, 2));
		}
	}
}

uint8_t pinState() {
	uint8_t state = 0;
	for(int i = 0; i < PIN_COUNT; i++) {
		if(!(outputs & 1 << i)) {
			bool pinValue = digitalRead(PINS[i]);
			state |= pinValue << i;
		}
	}
	return state;
}

int calculate(uint8_t* formula) {
	bool stack[MAX_FORMULA_SIZE] = { 0 };
	int stackLength = 1;
	while(*formula != 0) {
		if((*formula & ~0x10) < 0x0a) {
			int operand = *formula & ~0x10;
			stack[stackLength++] = operand < PIN_COUNT && (outputs & 1 << operand)
				? bool(*formula & 0x10) : bool(state & (1 << *formula));
		} else if(*formula < 0x0e) {
			switch(*formula) {
				case OP_NOT: stack[stackLength - 1] = !stack[stackLength - 1]; break;
				case OP_AND: --stackLength; stack[stackLength - 1] = stack[stackLength] & stack[stackLength - 1]; break;
				case OP_OR:  --stackLength; stack[stackLength - 1] = stack[stackLength] | stack[stackLength - 1]; break;
				case OP_XOR: --stackLength; stack[stackLength - 1] = stack[stackLength] ^ stack[stackLength - 1]; break;
			}
		} else if(*formula == 0x0f) {
			break;
		}
		formula++;
	}
	return stack[stackLength - 1];
}

void flush() {
	Serial.flush();
	SerialWeb.flush();
}

void trigger(int pin) {
	uint8_t msg =  0x80 | (state >> pin & 1) << 3 | pin;
	if(serialTriggers & 1 << pin) {
		Serial.write(msg);
	}
	if(serialWebTriggers & 1 << pin) {
		SerialWeb.write(msg);
	}
}

void handleInputChange(uint16_t oldState, uint16_t newState) {
	uint8_t affectedOutputs = 0x00;
	uint8_t changedPins = 0x00;
	for(int i = 0; i < PIN_COUNT; i++) {
		if(!(outputs & 1 << i) && (oldState & 1 << i) != (newState & 1 << i)) {
			affectedOutputs |= dependents[i];
			changedPins |= 1 << i;
		}
	}

	for(int i = 0; i < PIN_COUNT; i++) {
		bool changed = changedPins & 1 << i;

		if((outputs & 1 << i) && (affectedOutputs & 1 << i)) {
			bool value = calculate(formulas[i]);
			if(value != (state & 1 << i)) {
				digitalWrite(PINS[i], value);
				changed = true;
			}
		}

		if(changed) {
			trigger(i);
		}
	}

	flush();
}

void loop() {
	processStream(Serial);
	processStream(SerialWeb);

	uint8_t newState = ((outputs | 0x03c0) & state) | pinState();

	if(newState != state) {
		handleInputChange(state, newState);
		state = newState;
	}
}
