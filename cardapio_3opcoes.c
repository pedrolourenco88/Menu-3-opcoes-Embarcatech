#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "ssd1306.h"
#include "hardware/clocks.h"

// Definição dos pinos utilizados
#define SDA_PIN     14
#define SCL_PIN     15
#define BUTTON_PIN  22 // sw do joystick

// Definições para o buzzer e LEDs da música
#define BUZZER_PIN  21

// Definições para LEDs do menu
#define led_red     13
#define led_green   11
#define led_blue    12

// variaveis para pwm do led
const uint16_t PERIOD = 1000;   // Período do PWM (valor máximo do contador)
const float DIVIDER_PWM = 16.0; // Divisor fracional do clock para o PWM
const uint16_t LED_STEP = 50;   // Passo de incremento/decremento para o duty cycle do LED
uint16_t led_level = 100;       // Nível inicial do PWM (duty cycle)
uint slice_led_b, slice_led_r;  // Variáveis para armazenar os slices de PWM correspondentes aos LEDs
uint16_t led_level_r = 50;
uint16_t led_level_b = 50;

bool song = false;

// notinhas para o tema do mário
#define NOTE_E7  2637
#define NOTE_C7  2093
#define NOTE_G7  3136
#define NOTE_G6  1568
#define NOTE_E6  1319
#define NOTE_A6  1760
#define NOTE_A7  3520
#define NOTE_B6  1976
#define NOTE_F7  2794
#define NOTE_AS6 1865
#define NOTE_D7  2349

// Definições de notas musicais do tema do mario
const int melody[] = {
    NOTE_E7, NOTE_E7, 0, NOTE_E7,
    0, NOTE_C7, NOTE_E7, 0,
    NOTE_G7, 0, 0,  0,
    NOTE_G6, 0, 0, 0,
  
    NOTE_C7, 0, 0, NOTE_G6,
    0, 0, NOTE_E6, 0,
    0, NOTE_A6, 0, NOTE_B6,
    0, NOTE_AS6, NOTE_A6, 0,
  
    NOTE_G6, NOTE_E7, NOTE_G7,
    NOTE_A7, 0, NOTE_F7, NOTE_G7,
    0, NOTE_E7, 0, NOTE_C7,
    NOTE_D7, NOTE_B6, 0, 0,
  
    NOTE_C7, 0, 0, NOTE_G6,
    0, 0, NOTE_E6, 0,
    0, NOTE_A6, 0, NOTE_B6,
    0, NOTE_AS6, NOTE_A6, 0,
  
    NOTE_G6, NOTE_E7, NOTE_G7,
    NOTE_A7, 0, NOTE_F7, NOTE_G7,
    0, NOTE_E7, 0, NOTE_C7,
    NOTE_D7, NOTE_B6, 0, 0
};


const int tempo[] = {
    109, 109, 109, 109,
    109, 109, 109, 109,
    109, 109, 109, 109,
    109, 109, 109, 109,
  
    109, 109, 109, 109,
    109, 109, 109, 109,
    109, 109, 109, 109,
    109, 109, 109, 109,
  
    144, 144, 144,
    109, 109, 109, 109,
    109, 109, 109, 109,
    109, 109, 109, 109,
  
    109, 109, 109, 109,
    109, 109, 109, 109,
    109, 109, 109, 109,
    109, 109, 109, 109,
  
    144, 144, 144,
    109, 109, 109, 109,
    109, 109, 109, 109,
    109, 109, 109, 109,
};

ssd1306_t display;

// Variáveis globais para controle do menu
volatile int menu_index = 0;
volatile bool in_menu = true;  // True enquanto estamos no menu

int waitWithRead(int timeMS) {
    for(int i = 0; i < timeMS; i = i+100){
        if(gpio_get(BUTTON_PIN) == 0) {
            song = true;
           return 1;
        }
        sleep_ms(50);
    }
    return 0;
}
// Inicializa o display OLED
void init_display() {
    i2c_init(i2c1, 40000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    ssd1306_init(&display, 128, 64, 0x3C, i2c1);
    display.external_vcc = false;
}

// Desenha o menu na tela OLED  
void draw_menu() {
    ssd1306_clear(&display);
    ssd1306_draw_string(&display, 10, 6, 1, "Menu:");
    ssd1306_draw_string(&display, 10, 22, 1, "Joystick LED");
    ssd1306_draw_string(&display, 10, 38, 1, "Buzzer - Mario");
    ssd1306_draw_string(&display, 10, 54, 1, "LED RGB");
    ssd1306_draw_empty_square(&display, 5, 19 + (menu_index * 16), 110, 12); // equação das coordenadas multiplicadas pela altura do retangulo, para não precisar digitar e limpar sepre
    ssd1306_show(&display);
}

// Lê o valor do eixo Y do joystick para navegação no menu (caso não esteja em uma opção do menu)
void joystick_read() {
    if (!in_menu) return; 
    adc_select_input(0);
    uint coord_y = adc_read();
    if (coord_y < 200) { // Joystick para baixo
        menu_index = (menu_index + 1) % 3; //Isso impede que menu_index ultrapasse o número total de opções. Em vez de precisar de um if manual 
                                            //para redefinir a variável, a operação módulo (%) automaticamente redefine para 0 quando ultrapassa 2.
        draw_menu();
        sleep_ms(50);
    } else if (coord_y > 3800) { // Joystick para cima
        menu_index = (menu_index - 1 + 3) % 3;
        draw_menu();
        sleep_ms(50);
    }
}

// inicializa o pwm do buzzer
void pwm_init_buzzer(uint pin) {

    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
}

// Função para reproduzir um tom no buzzer utilizando PWM
int play_tone(uint buzzer_pin, uint frequency, uint duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(buzzer_pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;
    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(buzzer_pin, top / 2); // 50% duty cycle

    if (waitWithRead(duration_ms)) {
        pwm_set_gpio_level(buzzer_pin, 0); // Desliga o som após a duração
        return 1;
    }
    pwm_set_gpio_level(buzzer_pin, 0); // Desliga o som após a duração
    sleep_ms(50); // Pausa entre notas
    return 0;                        
}

// Função que reproduz a música
void music(uint pin) {   
    in_menu = false;  // Desativa a navegação enquanto a música toca
    while(song == false){

    for (int i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
            if (melody[i] == 0) {
                if (waitWithRead(tempo[i])) {
                    return;
                }
            } else {
                play_tone(pin, melody[i], tempo[i]);
                
            }
        }
    }
    in_menu = true; // Após tocar a música, reativa a navegação e redesenha o menu
    draw_menu();
    pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
    song = false;
    
}

// configuração e inicialização do pwm que contola o led
void pwm_init_led(uint pin){

    gpio_set_function(pin, GPIO_FUNC_PWM); // Configura o pino do LED para função PWM
    uint slice = pwm_gpio_to_slice_num(pin);    // Obtém o slice do PWM associado ao pino do LED
    pwm_set_clkdiv(slice, DIVIDER_PWM);    // Define o divisor de clock do PWM
    pwm_set_wrap(slice, PERIOD);           // Configura o valor máximo do contador (período do PWM)
    pwm_set_gpio_level(pin, led_level);    // Define o nível inicial do PWM para o pino do LED
    pwm_set_enabled(slice, true);          // Habilita o PWM no slice correspondente
}

// Função para configurar o PWM de um LED (genérica para azul e vermelho)
void setup_pwm_led(uint led, uint *slice, uint16_t level)
{
  gpio_set_function(led, GPIO_FUNC_PWM); // Configura o pino do LED como saída PWM
  *slice = pwm_gpio_to_slice_num(led);   // Obtém o slice do PWM associado ao pino do LED
  pwm_set_clkdiv(*slice, DIVIDER_PWM);   // Define o divisor de clock do PWM
  pwm_set_wrap(*slice, PERIOD);          // Configura o valor máximo do contador (período do PWM)
  pwm_set_gpio_level(led, level);        // Define o nível inicial do PWM para o LED
  pwm_set_enabled(*slice, true);         // Habilita o PWM no slice correspondente ao LED
}

//contola o ciclo do led alternando entre baixa e alta luminosidade
void pulse_led(uint pin){
    pwm_init_led(pin);
    in_menu = false;  // Desativa a navegação
    uint up_down = 1; // Variável para controlar se o nível do LED aumenta ou diminui

    while(gpio_get(BUTTON_PIN) != 0){
        pwm_set_gpio_level(pin, led_level); // Define o nível atual do PWM (duty cycle)
        sleep_ms(150);                     // Atraso 
        if (up_down)
        {
            led_level += LED_STEP; // Incrementa o nível do LED
            if (led_level >= PERIOD)
                up_down = 0; // Muda direção para diminuir quando atingir o período máximo
        }
        else
        {
            led_level -= LED_STEP; // Decrementa o nível do LED
            if (led_level <= LED_STEP)
                up_down = 1; // Muda direção para aumentar quando atingir o mínimo
        }
    }
    in_menu = true;
    draw_menu();
    pwm_set_gpio_level(pin, 0);
  
} 

// Função para ler os valores dos eixos do joystick (X e Y)
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value)
{
                                    // Leitura do valor do eixo Y do joystick
  adc_select_input(0);              // Seleciona o canal ADC para o eixo Y
  sleep_us(2);                      // Pequeno delay para estabilidade
  *vry_value = adc_read();          // Lê o valor do eixo Y (0-4095)
                                    // Leitura do valor do eixo X do joystick
  adc_select_input(1);              // Seleciona o canal ADC para o eixo X
  sleep_us(2);                      // Pequeno delay para estabilidade
  *vrx_value = adc_read();          // Lê o valor do eixo X (0-4095)
}

// Inicia a leitura do joystick e associa aos leds azul e vermelho
void joystick_led(){

    uint16_t vrx_value, vry_value;                          // Variáveis para armazenar os valores do joystick (eixos X e Y) e botão
    setup_pwm_led(led_blue, &slice_led_b, led_level_b);     // Configura o PWM para o LED azul
    setup_pwm_led(led_red, &slice_led_r, led_level_r);      // Configura o PWM para o LED vermelho
    in_menu = false;                                        // Desativa a navegação

    while (gpio_get(BUTTON_PIN) == 1)
    {
    joystick_read_axis(&vrx_value, &vry_value);             // Lê os valores dos eixos do joystick                                            
    pwm_set_gpio_level(led_blue, vrx_value);                // Ajusta o brilho do LED azul com o valor do eixo X
    pwm_set_gpio_level(led_red, vry_value);                 // Ajusta o brilho do LED vermelho com o valor do eixo Y                                                       
    sleep_ms(50);                                          // Espera 50 ms antes de repetir o ciclo
    }
    in_menu = true;
    draw_menu();
    pwm_set_gpio_level(led_blue, 0);                // Ajusta o brilho do LED azul com o valor do eixo X
    pwm_set_gpio_level(led_red, 0);                 // Ajusta o brilho do LED vermelho com o valor do eixo Y 
}

// Executa a opção selecionada no menu
void execute_option() {
    if (menu_index == 0) {
        joystick_led();               
    } else if (menu_index == 1) { 
        music(BUZZER_PIN);// reproduz a música
    } else if (menu_index == 2) {
        pulse_led(led_green);//inicia o ciclo de brilho do led
    }
}

int main() {
    stdio_init_all(); // Inicializa a comunicação serial
    
    // Inicializa o display OLED e desenha o menu
    init_display();
    draw_menu();
    
    // Inicializa o ADC para o joystick
    adc_init();
    adc_gpio_init(27);
    adc_gpio_init(26);
    
    // Inicializa os LEDs 
    gpio_init(led_blue);
    gpio_set_dir(led_blue, GPIO_OUT);
    gpio_init(led_red);
    gpio_set_dir(led_red, GPIO_OUT);
    gpio_init(led_green);
    gpio_set_dir(led_green, GPIO_OUT);
    
    // Inicializa o buzzer (para a música) usando PWM
    gpio_init(BUZZER_PIN);
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    pwm_init_buzzer(BUZZER_PIN);
    
    // Configuração do botão do joystick
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    
    while (1) {
        if (in_menu) {
            joystick_read(); // Lê o valor do joystick para navegação
        }
        if (gpio_get(BUTTON_PIN) == 0) { // Se o botão for pressionado
            while (gpio_get(BUTTON_PIN) == 0){// aguarda o botão ser solto
            sleep_ms(1);
            } 
            execute_option(); // Executa a opção selecionada
        }
    }  
    return 0;
}
