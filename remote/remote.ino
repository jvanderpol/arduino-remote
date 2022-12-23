#include <IRremote.h>

// #define LOCAL_DEBUG

#define IR_RECEIVER_PIN 2
#define IR_LED_PIN 3
#define RED_LED_PIN 4
#define CHANGE_INPUT_IR_COMMAND 0x25

#define PHOTOCELL_PIN A0

#ifdef LOCAL_DEBUG
#  define LOCAL_DEBUG_PRINT(...)    Serial.print(__VA_ARGS__)
#  define LOCAL_DEBUG_PRINTLN(...)  Serial.println(__VA_ARGS__)
#else
#  define LOCAL_DEBUG_PRINT(...) void()
#  define LOCAL_DEBUG_PRINTLN(...) void()
#endif

struct IrCommand {
  const String* command_name;
  const uint16_t address;
  const uint8_t command;
  const uint8_t raw_data_length;
  const uint16_t raw_data[];

  /*bool operator==(IrCommand const & rhs) const {
    return this->address == rhs.address &&
        this-> command == rhs.command &&
        this->raw_data_length == rhs.raw_data_length;
        this->raw_data == rhs.raw_data;
  }*/
};

// Protocol=KASEIKYO_DENON Address=0x314 Command=0x2D Raw-Data=0x5C2D3140
const String input_tv_name = "INPUT_TV";
IrCommand INPUT_TV = {
  &input_tv_name,
  /*address*/ 0x314,
  /*command*/ 0x2D,
  /*raw_data_length*/ 99,
  /*raw_data*/ { 3330, 1670, 430,420, 380, 470, 380, 1270, 380,470, 380, 1270, 380, 470, 380, 1270, 380,
                 470, 380,470, 330, 1320, 380, 470, 380, 420, 430, 1270, 380, 1270, 380, 470, 380, 470,
                 380, 420, 430, 420, 380, 470, 380, 420, 430, 420, 430, 420, 380, 1270, 430, 420, 380,
                 1270, 430, 420, 380, 470, 380, 470, 380, 1270, 380, 1270, 430, 420, 380, 470, 380, 1270,
                 380, 470, 380, 1270, 430, 1220, 430, 420, 430, 1270, 380, 420, 430, 420, 380, 470, 380,
                 470, 380, 1270, 380, 1270, 430, 1270, 380, 420, 430, 1270, 330, 470, 430}
};

// Protocol=KASEIKYO_DENON Address=0x614 Command=0x2D Raw-Data=0xC2D6140
const String input_chromecast_name = "INPUT_CHROMECAST";
IrCommand INPUT_CHROMECAST = {
  &input_chromecast_name,
  /*address*/ 0x614,
  /*command*/ 0x2D,
  /*raw_data_length*/ 99,
  /*raw_data*/ {3280, 1720, 380, 470, 330, 470, 380, 1320, 380, 420, 380, 1320, 380, 470, 330, 1320, 380,
                470, 330, 470, 380, 1320, 380, 420, 380, 470, 380, 1320, 330, 1320, 380, 470, 380, 420, 380,
                470, 380, 470, 380, 470, 330, 470, 380, 470, 380, 470, 380, 1270, 380, 470, 380, 1270, 380,
                470, 380, 470, 380, 420, 380, 470, 380, 1320, 330, 1320, 380, 470, 380, 1270, 380, 470, 380,
                1270, 380, 1270, 380, 470, 380, 1320, 330, 470, 380, 470, 380, 470, 380, 420, 380, 1320, 380,
                1270, 380, 470, 380, 470, 380, 420, 380, 470, 380}
};

const String input_unknown = "INPUT_UNKNOWN";
IrCommand INPUT_UNKNOWN = {
  &input_unknown,
  /*address*/ 0x0,
  /*command*/ 0x0,
  /*raw_data_length*/ 99,
  /*raw_data*/ {}
};

// Protocol=DENON Address=0x8 Command=0x9E Raw-Data=0x2278
const String receiver_power_on_name = "RECEIVER_POWER_ON";
IrCommand RECEIVER_POWER_ON = {
  &receiver_power_on_name,
  /*address*/ 0x8,
  /*command*/ 0x9E,
  /*raw_data_length*/ 0,
  /*raw_data*/ {}
};

// Protocol=DENON Address=0x8 Command=0x5E Raw-Data=0x2178 15 bits MSB first
const String receiver_power_off_name = "RECEIVER_POWER_OFF";
IrCommand RECEIVER_POWER_OFF = {
  &receiver_power_off_name,
  /*address*/ 0x8,
  /*command*/ 0x5E,
  /*raw_data_length*/ 0,
  /*raw_data*/ {}
};

#define POWER_OFF 0
#define POWER_ON 1
#define POWER_UNKNOWN 2
#define COMMAND_QUEUE_LENGTH 5

#define MIN_IR_WAIT_TIME 300

uint8_t tvOn = POWER_UNKNOWN;
uint8_t receiverOn = POWER_UNKNOWN;
IrCommand* receiverInput = &INPUT_UNKNOWN;

unsigned long lastIrSendTime = 0;
unsigned long lastIrReceiveTime = 0;
IrCommand *commandQueue[COMMAND_QUEUE_LENGTH] = {&INPUT_UNKNOWN, &INPUT_UNKNOWN, &INPUT_UNKNOWN, &INPUT_UNKNOWN, &INPUT_UNKNOWN};
// int8_t nextCommandIndex = -1;
int8_t endOfQueueIndex = -1;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  //pinMode(RED_LED_PIN, OUTPUT);
  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(IR_RECEIVER_PIN, INPUT);
  pinMode(PHOTOCELL_PIN, INPUT);
  #ifdef LOCAL_DEBUG
  Serial.begin(9600);
  #endif
  IrReceiver.begin(
    IR_RECEIVER_PIN,
    /*ENABLE_LED_FEEDBACK=*/false,
    /*USE_DEFAULT_FEEDBACK_LED_PIN=*/false);
  IrSender.begin(IR_LED_PIN, /*ENABLE_LED_FEEDBACK=*/true);
  LOCAL_DEBUG_PRINTLN("Setup Complete");
}

// the loop function runs over and over again forever
void loop() {
  processIrInput();
  checkTvState();
  maybeTurnOffReceiver();
  sendQueuedCommands();
}

void processIrInput() {
  if (IrReceiver.decode()) {
    #ifdef LOCAL_DEBUG
    IrReceiver.printIRResultShort(&Serial);
    #endif
    if (IrReceiver.decodedIRData.protocol != UNKNOWN || IrReceiver.decodedIRData.numberOfBits > 8) {
      lastIrReceiveTime = millis();
    } else {
      LOCAL_DEBUG_PRINTLN("Ignoring invalid command");
    }
    // if (IrReceiver.decodedIRData.command == CHANGE_INPUT_IR_COMMAND && nextCommandIndex < 0 && tvOn) {
    if (IrReceiver.decodedIRData.command == CHANGE_INPUT_IR_COMMAND &&
        endOfQueueIndex < 0 &&
        tvOn) {
      if (receiverInput == &INPUT_CHROMECAST) {
        LOCAL_DEBUG_PRINTLN("Change to TV");
        setReceiverInput(&INPUT_TV);
      } else {
        LOCAL_DEBUG_PRINTLN("Change to chromecast");
        setReceiverInput(&INPUT_CHROMECAST);
      }
    }
    IrReceiver.resume();
  }
}

void checkTvState() {
  int newTvOn = analogRead(PHOTOCELL_PIN) > 300;
  if (newTvOn != tvOn) {
    tvOn = newTvOn;
    LOCAL_DEBUG_PRINT("TV on: ");
    LOCAL_DEBUG_PRINTLN(tvOn ? "true" : "false");
    if (tvOn) {
      trySendIrCommand(&RECEIVER_POWER_ON);
      trySendIrCommand(&RECEIVER_POWER_ON);
    } else {
      trySendIrCommand(&RECEIVER_POWER_OFF);
      trySendIrCommand(&RECEIVER_POWER_OFF);
      trySendIrCommand(&RECEIVER_POWER_OFF);
    }
  }
}

void maybeTurnOffReceiver() {
  if (!tvOn &&
      abs(millis() - lastIrSendTime) > 60000 * 5) {
    LOCAL_DEBUG_PRINTLN("Turn off receiver every 5 minutes");
    trySendIrCommand(&RECEIVER_POWER_OFF);
    trySendIrCommand(&RECEIVER_POWER_OFF);
  }
}

void sendQueuedCommands() {
  // if (canSendIrCommand() && nextCommandIndex >= 0) {
  if (canSendIrCommand() && endOfQueueIndex >= 0) {
    // sendIrCommand(commandQueue[nextCommandIndex]);
    // nextCommandIndex--;
    sendIrCommand(commandQueue[0]);
    for (int i = 1; i < endOfQueueIndex; i++) {
      commandQueue[i - 1] = commandQueue[i];
    }
    endOfQueueIndex--;
  }  
}

void printRaw() {
  IrReceiver.compensateAndPrintIRResultAsCArray(&Serial, true);
}

void setReceiverInput(IrCommand* input) {
  receiverInput = input;
  trySendIrCommand(input);
  trySendIrCommand(input);
}

void trySendIrCommand(IrCommand* command) {
  if (canSendIrCommand()) {
    sendIrCommand(command);
  // } else if (nextCommandIndex < COMMAND_QUEUE_LENGTH - 1) {
  } else if (endOfQueueIndex < COMMAND_QUEUE_LENGTH - 1) {
    LOCAL_DEBUG_PRINT("Queue command ");
    LOCAL_DEBUG_PRINTLN(*command->command_name);
    // nextCommandIndex++;
    // commandQueue[nextCommandIndex] = command;
    endOfQueueIndex++;
    commandQueue[endOfQueueIndex] = command;
  } else {
    LOCAL_DEBUG_PRINTLN("Ignore command due to full queue");
  }
}

bool canSendIrCommand() {
   return abs(millis() - max(lastIrSendTime, lastIrReceiveTime)) > MIN_IR_WAIT_TIME;
}

void sendIrCommand(IrCommand* command) {
  digitalWrite(RED_LED_PIN, HIGH);
  if (command->raw_data_length > 0) {
    LOCAL_DEBUG_PRINTLN("Send raw");
    IrSender.sendRaw(command->raw_data, command->raw_data_length, 37);
  } else {
    LOCAL_DEBUG_PRINT("Send ");
    LOCAL_DEBUG_PRINT(*command->command_name);
    LOCAL_DEBUG_PRINT(" (address: ");
    LOCAL_DEBUG_PRINT(command->address, HEX);
    LOCAL_DEBUG_PRINT(" command: ");
    LOCAL_DEBUG_PRINT(command->command, HEX);
    LOCAL_DEBUG_PRINTLN(")");
    IrSender.sendDenon(command->address, command->command, /*repeats=*/ 3, false);
  }
  digitalWrite(RED_LED_PIN, LOW);
  lastIrSendTime = millis();
}
