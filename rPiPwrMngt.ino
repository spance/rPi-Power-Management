/**
 * The state machine:
 * 
 *                   ┌─────────────┐
 *                   │  POWER_OFF  │
 *                   └──────┬──────┘
 *                          │ <press button>
 *                     ┌────┴─────┐
 *                     │ POWER_ON ├────────────────────────────────────┐
 *                     └────┬─────┘                                    │
 *                          │                                          │
 *               ┌──────────┴───────────┐                              │
 *  <long press> │                      │ <serial command>             │
 *               │             ┌────────┴───────────┐                  │
 *               │             │  POWER_OFF_PENDING │                  │
 *               │             └────────┬───────────┘                  │
 *               │                      │                              │
 *               │       <timer>        │       <press button>         │
 *               │     ┌────────────────┴──────────────────────────────┘
 *               │     │                
 *          ┌────┴─────┴────┐                          
 *          │   POWER_OFF   │                 
 *          └───────────────┘
 * 
 * 
 **/

// the pin number of relay
const int PIN_RELAY = 7;
// the pin number of button
const int PIN_POWER_BTN = 4;

// max uint32
const uint32_t MAX_U32 = 0xFFffFFffL;
// power off keyword for checking kernel logs
// e.g. [  145.311488] reboot: Power down
const String POWER_OFF_KEY = "Power down";

// multiple operations may be ignored, __ms
const uint32_t ACTION_IGNORED_TIME = 2000L;
// the min duration of long press button, __ms
const uint32_t LONG_PRESS_THRESHOLD = 3000L;
// the min duration of short press button, __ms
const uint32_t SHORT_PRESS_THRESHOLD = 15L;
// the delay of pending to power off, __second
const int POWER_OFF_PENDING_DELAY = 20;

const int KEY_PRESSED = 1;
const int KEY_PRESSED_LONG = 2;

// 当前电源状态
int powerState = 0;
// 延迟器设置的剩余等待时间
long waitingTime = 0;
// 延迟器最后一次操作时间
uint32_t lastTime = 0;
// 电源状态最后一次操作时间
uint32_t lastOperation = 0;

// Power state enum off=0
enum PWR {
    POWER_OFF,
    POWER_OFF_PENDING,
    POWER_ON,
};

/**
 * 设置秒级延迟
 * 记录延迟起点时间戳=lastTime
 * 记录延迟时长=waitingTime in seconds
 **/
void setupTimer(int seconds)
{
    uint32_t t0 = millis();
    waitingTime = seconds * 1000L;
    lastTime = t0;
}

/**
 * 检查延迟是否到期，支持在 uint32 时间戳溢出后（大约50天）的下一周期的延迟判断
 **/
bool checkTimer()
{
    uint32_t t0 = millis();
    uint32_t delta = 0;
    if (t0 < lastTime) { // overflow
        // 通常情况下上一次执行 lastTime=MAX_U32
        // 本次执行则 t0=0
        // 跨越溢出点后时长 +1
        delta = MAX_U32 - lastTime;
        delta += t0 + 1;
    } else {
        delta = t0 - lastTime;
    }

    if (delta >= 8) {
        // 防止扩大计时误差 当时间流逝超过_ms后才更新 lastTime waitingTime
        lastTime = t0;
        waitingTime -= delta;
    }
    return waitingTime <= 0;
}

/**
 * 检查 button 是否按下，返回 短按=1 长按=2
 **/
int checkButton()
{
    uint32_t t0 = millis();
    uint32_t t1 = MAX_U32;
    int state;

    for (int i = 0;; i++) {
        state = digitalRead(PIN_POWER_BTN);
        if (state == HIGH) { // 没有按键或按键弹起
            // 不合理的过短的按键行为
            if (i == 0 || t1 - t0 < SHORT_PRESS_THRESHOLD) {
                return 0;
            }
            return KEY_PRESSED;
        }

        t1 = millis();
        // 已经按下足够长时间
        if (t1 - t0 >= LONG_PRESS_THRESHOLD) {
            return KEY_PRESSED_LONG;
        }
    }
}

/**
 * 在长按后等待按键释放
 **/
void waitButtonRelease()
{
    for (int state; (state = digitalRead(PIN_POWER_BTN)) == LOW;) {
        delay(1);
    }
}

/**
 * 从串口rx检查 是否得到关机指令
 **/
bool checkPowerOff()
{
    if (Serial.available() > 0) {
        String msgRx = Serial.readString();
        // 回写到Tx
        Serial.println(msgRx);
        // 检查串口数据
        return msgRx.indexOf(POWER_OFF_KEY) >= 0;
    }
    return false;
}

/**
 * 操作电源开闭状态
 **/
void turnPowerState(int state)
{
    uint32_t t = millis();
    uint32_t delta = t - lastOperation;
    if (lastOperation != 0) {
        if (t < lastOperation) { // t已经溢出
            delta = MAX_U32 - lastOperation + t;
        }

        if (delta < ACTION_IGNORED_TIME) { // t以内再次操作则忽略
            Serial.print(state);
            Serial.println("# operation ignored ");
            goto end;
        }
    }

    switch (state) {
    case POWER_ON:
        Serial.println("power on");
        digitalWrite(PIN_RELAY, HIGH);
        powerState = POWER_ON;
        break;

    case POWER_OFF:
        Serial.println("power off");
        digitalWrite(PIN_RELAY, LOW);
        powerState = POWER_OFF;
        break;

    case POWER_OFF_PENDING:
        Serial.println("power off pending");
        powerState = POWER_OFF_PENDING;
        setupTimer(POWER_OFF_PENDING_DELAY);
        break;
    }

end:
    lastOperation = t;
}

void setup()
{
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_POWER_BTN, INPUT_PULLUP);

    //Initialize serial and wait for port to open:
    Serial.begin(115200);
    // while (!Serial) {
    //     ; // wait for serial port to connect. Needed for native USB
    // }

    // 读串口rx 指令带有超时
    Serial.setTimeout(100);
}

void loop()
{
    switch (powerState) {

    case POWER_ON:
        // 检查是否强制关机
        if (checkButton() == KEY_PRESSED_LONG) {
            turnPowerState(POWER_OFF);
            waitButtonRelease();
            break;
        }

        // 第二检查， 是否串口指令延时关机
        if (checkPowerOff()) {
            turnPowerState(POWER_OFF_PENDING);
            break;
        }

        break;

    case POWER_OFF:
        // 是否 button 触发开机
        if (checkButton() == KEY_PRESSED) {
            turnPowerState(POWER_ON);
            break;
        }

        break;

    case POWER_OFF_PENDING:
        // 检查是否按钮取消延迟关机
        if (checkButton() == KEY_PRESSED) {
            turnPowerState(POWER_ON);
            break;
        }

        // 第二检查， 定时器到期
        if (checkTimer()) {
            turnPowerState(POWER_OFF);
            break;
        }

        break;
    }
}
