#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "ssd1306.h"

#define BEEP_PIN 27
#define TEMP_PIN 26
#define TEMP_ADC 0
#define INC_PIN 0
#define DEC_PIN 1


bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}



ssd1306_t disp;
void init() {
    stdio_init_all();

    gpio_init(BEEP_PIN);
    gpio_set_dir(BEEP_PIN, GPIO_OUT);

    adc_init();
    adc_gpio_init(TEMP_PIN);
    adc_select_input(TEMP_ADC);

    gpio_init(INC_PIN);
    gpio_set_dir(INC_PIN, GPIO_IN);
    gpio_pull_up(INC_PIN);
    gpio_init(DEC_PIN);
    gpio_set_dir(DEC_PIN, GPIO_IN);
    gpio_pull_up(DEC_PIN);

    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);
}

void beep_set(bool enabled) {
    gpio_put(BEEP_PIN, enabled);
}

bool inc_get(){
    return !gpio_get(INC_PIN);
}

bool dec_get(){
    return !gpio_get(DEC_PIN);
}

double set_temp = 60;

#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128
#define FONT_HEIGHT 8
#define FONT_WIDTH 5
#define SCREEN_CHAR_HEIGHT (SCREEN_HEIGHT/FONT_HEIGHT)
#define SCREEN_CHAR_WIDTH (SCREEN_WIDTH/FONT_WIDTH)

#define OLED_BUFFER_SIZE (SCREEN_CHAR_HEIGHT * SCREEN_CHAR_WIDTH)
char oled_buffer[OLED_BUFFER_SIZE];
#define oled_ui(...) {snprintf(oled_buffer, OLED_BUFFER_SIZE, __VA_ARGS__);flush_oled_buffer();}
void flush_oled_buffer() {
  ssd1306_clear(&disp);
  unsigned char y = 0;
  unsigned char x = 0;
  for (size_t i = 0; oled_buffer[i]; i++) {
    if (oled_buffer[i] == '\r')
      continue;
    if (oled_buffer[i] == '\t')
      continue; // TODO
    if (oled_buffer[i] == '\n') {
      y++;
      x = 0;
      continue;
    }
    if (x > (SCREEN_WIDTH-FONT_WIDTH)) {
      y++;
      x = 0;
    }
    if (y >= SCREEN_CHAR_HEIGHT)
      break;
    ssd1306_draw_char(&disp, x, y*FONT_HEIGHT, 1, oled_buffer[i]);
    x += FONT_WIDTH;
  }
  for (;;) {
    if (x > (SCREEN_WIDTH-FONT_WIDTH)) {
      y++;
      x = 0;
    }
    if (y >= SCREEN_CHAR_HEIGHT)
      break;
    ssd1306_draw_char(&disp, x, y*FONT_HEIGHT, 1, ' ');
    x += FONT_WIDTH;
  }
  ssd1306_show(&disp);
}

double adc_u16_to_voltage(uint16_t result){
    // 12-bit conversion, assume max value == ADC_VREF == 3.3 V
    const double conversion_factor = 3.3f / (1 << 12);
    return result * conversion_factor;
}

double adc_d_to_voltage(double result){
    // 12-bit conversion, assume max value == ADC_VREF == 3.3 V
    const double conversion_factor = 3.3f / (1 << 12);
    return result * conversion_factor;
}

#define ADC_BUFFER_SIZE 64
uint16_t adc_buffer[ADC_BUFFER_SIZE];
size_t adc_buffer_pointer;

void adc_tick() {
    if(adc_buffer_pointer>=ADC_BUFFER_SIZE) adc_buffer_pointer=0;
    adc_buffer[adc_buffer_pointer] = adc_read();
    adc_buffer_pointer++;
}

double adc_get_d() {
    uint64_t sum=0;
    for(size_t i=0;i<ADC_BUFFER_SIZE;i++) {
        sum+=adc_buffer[i];
    }
    return ((double)sum)/((double)ADC_BUFFER_SIZE);
}

// https://blog.csdn.net/u013866683/article/details/79391849
//温度系数
#define B 3950.0
//额定温度(绝对温度加常温:273.15+25)
#define TN 298.15
// 额定阻值(绝对温度时的电阻值10k)
#define RN 10
#define BaseVol 3.3
double temp_read() {
    double RV = adc_d_to_voltage(adc_get_d());
    double RT=RV*10/(BaseVol-RV);//求出当前温度阻值 (BaseVoltage-RV)/R16=RV/RT;
    double Tmp=1/(1/TN+(log(RT/RN)/B))-273.15;//%RT = RN exp*B(1/T-1/TN)%
    return Tmp;
}

double temp;
// 0.01s
#define CYCLE 10
int main() {
    init();
    uint64_t count=0;
    for(;;) {
        sleep_ms(CYCLE);
        count++;
        adc_tick();

        // every 0.3s
        if(count%(300/CYCLE)==0){
            temp = temp_read();
            beep_set(temp>set_temp);
        }

        // every 0.2s
        if(count%(200/CYCLE)==0){
            if(inc_get()){
                set_temp+=0.1;
            }
            if(dec_get()){
                set_temp-=0.1;
            }
        }

        // every 0.05s
        if(count%(50/CYCLE)==0){
            oled_ui("Welcome to the Temperature Measurement Device.\n"
            "Temperature =\n%lf\n"
            "Warning Temperature =\n%lf\n",
            temp,set_temp);
        }
        
    }
    return 0;
}