/*
 * app_chassis.c
 * -----------------------------------------------------------------------------
 * 底盘速度控制。
 *
 * 速度档位：共 7 档，第 3 档为中间档。档位只保存"序号"，目标转速由序号
 * 换算得到，所以不存在按键按住一直累加的问题。
 *
 * 速度环：增量式 PI，每个控制周期只累加"偏差的变化量"，天然带积分记忆，
 * 不需要额外保存积分项，参数也更容易调。
 */

#include "app_chassis.h"
#include "app_config.h"
#include "motor.h"
#include "encoder.h"

/* 每个轮子的 PI 状态 */
typedef struct {
    float bias;        /* 本周期偏差 */
    float last_bias;   /* 上周期偏差 */
    float output;      /* 累计输出 */
} pi_state_t;

/* ★ 当前档位序号与当前目标车速。上电时分别为中间档 3 和 GEAR_BASE_RPM，
   想改初始速度不要去动这里，改 app_config.h 的 GEAR_BASE_RPM 即可。 */
static uint8_t      s_gear      = GEAR_CENTER;      /* 当前档位 0~6 */
static int          s_speed_set = GEAR_BASE_RPM;    /* 当前档位的目标转速 rpm */

/* ★ 叶片电机档位：同样只存序号，占空比由序号换算，按一次只动一档 */
static uint8_t      s_aux_gear  = AUX_GEAR_BOOT;    /* 叶片电机档位 0~6 */
static float        s_target[WHEEL_NUM];
static float        s_actual[WHEEL_NUM];
static int          s_duty[WHEEL_NUM];
static pi_state_t   s_pi[WHEEL_NUM];

/* 编码器脉冲 -> 电机轴 rpm */
static float pulses_to_rpm(int pulses)
{
    return (float)pulses / (MOTOR_GEAR_RATIO * ENCODER_PULSE_PER_REV * ENCODER_EDGE_MULT)
           * CONTROL_RATE_HZ * 60.0f;
}

static int pi_step(pi_state_t *pi, float actual, float target)
{
    pi->bias = target - actual;
    pi->output += SPEED_PI_KP * (pi->bias - pi->last_bias) + SPEED_PI_KI * pi->bias;
    pi->last_bias = pi->bias;

    if (pi->output > PWM_OUTPUT_LIMIT) {
        pi->output = PWM_OUTPUT_LIMIT;
    }
    if (pi->output < -PWM_OUTPUT_LIMIT) {
        pi->output = -PWM_OUTPUT_LIMIT;
    }

    return (int)pi->output;
}

/* ---------------------------------------------------- 叶片电机档位换算 */

/* 档位序号 -> 占空比：最高档 AUX_GEAR_TOP_DUTY，往下每档减 AUX_GEAR_STEP */
static uint8_t aux_gear_to_duty(uint8_t gear)
{
    int duty = AUX_GEAR_TOP_DUTY - ((AUX_GEAR_COUNT - 1) - (int)gear) * AUX_GEAR_STEP;

    if (duty < 0) {
        duty = 0;
    }
    if (duty > 100) {
        duty = 100;                 /* 占空比物理上限 */
    }

    return (uint8_t)duty;
}

void chassis_init(void)
{
    uint8_t i;

    /* 上电复位到中间档，目标转速取 GEAR_BASE_RPM（初始速度的唯一来源） */
    s_gear      = GEAR_CENTER;
    s_speed_set = GEAR_BASE_RPM;

    /* 叶片电机复位到上电档位（默认第 6 档 = 100%），并立刻按该档输出 */
    s_aux_gear = AUX_GEAR_BOOT;
    aux_motor_set_duty(aux_gear_to_duty(s_aux_gear));

    for (i = 0; i < WHEEL_NUM; i++) {
        s_target[i] = 0.0f;
        s_actual[i] = 0.0f;
        s_duty[i]   = 0;

        s_pi[i].bias      = 0.0f;
        s_pi[i].last_bias = 0.0f;
        s_pi[i].output    = 0.0f;
    }
}

/*
 * 升 / 降一档：direction 传 +1（升）或 -1（降）。
 * 这里只改"档位序号"，再由序号重新算出目标转速，所以按一次只动一档，
 * 不会像旧版那样把速度值一直累加上去。
 * 到顶（>=7）或到底（<0）时回落到中间档，形成 7 档循环。
 */
void chassis_gear_change(int8_t direction)
{
    int gear = (int)s_gear + (int)direction;

    /* 越界即回落中间档，形成循环 */
    if (gear < 0 || gear >= GEAR_COUNT) {
        gear = GEAR_CENTER;
    }

    s_gear      = (uint8_t)gear;

    /* 下面就这一行把档位序号换算成目标车速，基准值取自 app_config.h 的 GEAR_BASE_RPM */
    s_speed_set = GEAR_BASE_RPM + ((int)s_gear - GEAR_CENTER) * GEAR_STEP_RPM;
}

uint8_t chassis_gear(void)
{
    return s_gear;
}

/* ---------------------------------------------------- 叶片电机档位对外 */

/* 升 / 降一档：+1 升档、-1 降档；到顶（>=7）或到底（<0）回落中间档 */
void aux_gear_change(int8_t direction)
{
    int gear = (int)s_aux_gear + (int)direction;

    if (gear < 0 || gear >= AUX_GEAR_COUNT) {
        gear = AUX_GEAR_CENTER;
    }

    s_aux_gear = (uint8_t)gear;

    /* 改完立刻按新档位的占空比输出，不需要等下一个控制周期 */
    aux_motor_set_duty(aux_gear_to_duty(s_aux_gear));
}

uint8_t aux_gear(void)
{
    return s_aux_gear;
}

uint8_t aux_gear_duty(void)
{
    return aux_gear_to_duty(s_aux_gear);
}

int chassis_speed_set(void)
{
    return s_speed_set;
}

static void set_all_targets(float a, float b, float c, float d)
{
    s_target[WHEEL_A] = a;
    s_target[WHEEL_B] = b;
    s_target[WHEEL_C] = c;
    s_target[WHEEL_D] = d;
}

void chassis_apply_command(uint8_t dir_key)
{
    float speed = (float)s_speed_set;
    float fast  = speed * TURN_RATE_SCALE;         /* 转向时外圈轮 */
    float slow  = speed * TURN_RATE_SCALE / 2.0f;  /* 转向时内圈轮 */

    switch (dir_key) {
        case 1:   /* 前进 */
            set_all_targets(speed, speed, speed, speed);
            break;

        case 2:   /* 右前 */
            set_all_targets(fast, slow, fast, slow);
            break;

        case 3:   /* 原地右转 */
            set_all_targets(fast, -fast, fast, -fast);
            break;

        case 4:   /* 右后 */
            set_all_targets(-fast, -slow, -fast, -slow);
            break;

        case 5:   /* 后退 */
            set_all_targets(-speed, -speed, -speed, -speed);
            break;

        case 6:   /* 左后 */
            set_all_targets(-slow, -fast, -slow, -fast);
            break;

        case 7:   /* 原地左转 */
            set_all_targets(-fast, fast, -fast, fast);
            break;

        case 8:   /* 左前 */
            set_all_targets(slow, fast, slow, fast);
            break;

        case 20:  /* 叶片电机档位键（T/U）：在解码阶段已按 200ms 去抖处理，这里不再动作 */
        case 21:
            break;

        case 24:  /* 升档：解码时已经改好档位，这里不需要动作 */
        case 25:  /* 降档 */
            break;

        default:  /* 摇杆松开，四轮目标清零 */
            set_all_targets(0.0f, 0.0f, 0.0f, 0.0f);
            break;
    }
}

void chassis_control_step(void)
{
    int pulses[WHEEL_NUM];
    uint8_t i;

    /* 右侧两路编码器相位相反，取负号后与左侧同向 */
    pulses[WHEEL_A] =  encoder_read(ENC_WHEEL_A);
    pulses[WHEEL_B] = -encoder_read(ENC_WHEEL_B);
    pulses[WHEEL_C] =  encoder_read(ENC_WHEEL_C);
    pulses[WHEEL_D] = -encoder_read(ENC_WHEEL_D);

    for (i = 0; i < WHEEL_NUM; i++) {
        s_actual[i] = pulses_to_rpm(pulses[i]);
        s_duty[i]   = pi_step(&s_pi[i], s_actual[i], s_target[i]);

        /* 输出再钳一次，保证落在 PWM 满量程以内 */
        if (s_duty[i] > MOTOR_PWM_ARR) {
            s_duty[i] = MOTOR_PWM_ARR;
        }
        if (s_duty[i] < -MOTOR_PWM_ARR) {
            s_duty[i] = -MOTOR_PWM_ARR;
        }
    }

    motor_set_duty(s_duty[WHEEL_A], s_duty[WHEEL_B], s_duty[WHEEL_C], s_duty[WHEEL_D]);
}

float chassis_target_rpm(chassis_wheel_t wheel)
{
    if (wheel >= WHEEL_NUM) {
        return 0.0f;
    }

    return s_target[wheel];
}

float chassis_actual_rpm(chassis_wheel_t wheel)
{
    if (wheel >= WHEEL_NUM) {
        return 0.0f;
    }

    return s_actual[wheel];
}
