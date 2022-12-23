#include <IRremote.h>

#define IR_RECEIVER_PIN 7
#define IR_LED_PIN 4
#define RED_LED_PIN 12
#define BUTTON_PIN 2

#define PHOTOCELL_PIN A0

struct IrCommand {
  //uint_fast8_t repeats, 
  uint16_t address;
  uint8_t command;
  bool kaseikyo;

  bool operator==(IrCommand const & rhs) const {
    return this->address == rhs.address &&
        this-> command == rhs.command &&
        this->kaseikyo == rhs.kaseikyo;
  }
};

// Protocol=KASEIKYO_DENON Address=0x314 Command=0x2D Raw-Data=0x5C2D3140
IrCommand INPUT_TV = {
  /*address*/ 0x314,
  /*command*/ 0x2D,
  /*kaseikyo*/ true
};
// Protocol=KASEIKYO_DENON Address=0x614 Command=0x2D Raw-Data=0xC2D6140
IrCommand INPUT_CHROMECAST = {
  /*address*/ 0x614,
  /*command*/ 0x2D,
  /*kaseikyo*/ true
};
IrCommand INPUT_UNKNOWN = {
  /*address*/ 0x0,
  /*command*/ 0x0,
  /*kaseikyo*/ true
};

// Protocol=DENON Address=0x8 Command=0x9E Raw-Data=0x2278
IrCommand RECEIVER_POWER_ON = {
  /*address*/ 0x8,
  /*command*/ 0x9E,
  /*kaseikyo*/ false
};
// Protocol=DENON Address=0x8 Command=0x5E Raw-Data=0x2178 15 bits MSB first

IrCommand RECEIVER_POWER_OFF = {
  /*address*/ 0x8,
  /*command*/ 0x5E,
  /*kaseikyo*/ false
};

#define POWER_OFF 0
#define POWER_ON 1
#define POWER_UNKNOWN 2

#define MIN_IR_WAIT_TIME 2000

int tvOn = POWER_UNKNOWN;
int receiverOn = POWER_UNKNOWN;
IrCommand receiverInput = INPUT_UNKNOWN;
int lastIrSendTime = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(IR_RECEIVER_PIN, INPUT);
  pinMode(PHOTOCELL_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT);
  Serial.begin(9600);
  IrReceiver.begin(
    IR_RECEIVER_PIN,
    /*ENABLE_LED_FEEDBACK=*/false,
    /*USE_DEFAULT_FEEDBACK_LED_PIN=*/false);
  IrSender.begin(IR_LED_PIN, /*ENABLE_LED_FEEDBACK=*/true);
}

// the loop function runs over and over again forever
void loop() {
  if (IrReceiver.decode()) {
    IrReceiver.printIRResultShort(&Serial);
    if (IrReceiver.decodedIRData.command == 0x25 &&
        abs(millis() - lastIrSendTime) > MIN_IR_WAIT_TIME) {
       delay(1000);
      if (receiverInput == INPUT_CHROMECAST) {
        setReceiverInput(INPUT_TV);
      } else {
        setReceiverInput(INPUT_CHROMECAST);
      }
    }
    IrReceiver.resume();
  }
  int newTvOn = analogRead(PHOTOCELL_PIN) > 300;
  if (newTvOn != tvOn) {
    tvOn = newTvOn;
    Serial.print("TV on: ");
    Serial.println(tvOn ? "true" : "false");
    if (tvOn) {
      sendIrCommand(RECEIVER_POWER_ON);
    } else {
      sendIrCommand(RECEIVER_POWER_OFF);
    }
  }
  if (digitalRead(BUTTON_PIN) == HIGH && abs(millis() - lastIrSendTime) > MIN_IR_WAIT_TIME) {
    sendIrCommand(RECEIVER_POWER_ON);
  }
}

void setReceiverInput(IrCommand& input) {
  receiverInput = input;
  sendIrCommand(input);
}

void sendIrCommand(IrCommand& command) {
  Serial.print("Send address: ");
  Serial.print(command.address, HEX);
  Serial.print(" command: ");
  Serial.print(command.command, HEX);
  Serial.print(" kaseikyo: ");
  Serial.println(command.kaseikyo);
  digitalWrite(RED_LED_PIN, HIGH);
  if (command.kaseikyo) {
    IrSender.sendKaseikyo(command.address, command.command, /*repeats=*/ 3, DENON_VENDOR_ID_CODE);
  } else {
    IrSender.sendDenon(command.address, command.command, /*repeats=*/ 3, false);
  }
  delay(50); // Possibly needed to keep messages distinct
  digitalWrite(RED_LED_PIN, LOW);
  lastIrSendTime = millis();
}
