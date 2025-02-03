#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include "ws2812.pio.h"

#define IS_RGBW false
#define NUM_PIXELS 25
#define WS2812_PIN 7
#define Tempo_Delay 200000

#define Led_R 13
#define Led_G 11
#define Led_B 12
#define Button_0 5
#define Button_1 6

static volatile uint desenho_atual = 0;

// Buffer para armazenar quais LEDs estão ligados matriz 5x5
bool led_buffer[10][NUM_PIXELS] = {
    //número 0
    {0, 1, 1, 1, 0, 
     0, 1, 0, 1, 0,                 
     0, 1, 0, 1, 0, 
     0, 1, 0, 1, 0, 
     0, 1, 1, 1, 0},

    //número 1
    {0, 0, 1, 0, 0, 
     0, 0, 1, 0, 0, 
     0, 0, 1, 0, 0, 
     0, 0, 1, 0, 0, 
     0, 0, 1, 1, 0},

    //número 2
    {0, 1, 1, 1, 0, 
     0, 1, 0, 0, 0, 
     0, 1, 1, 1, 0, 
     0, 0, 0, 1, 0, 
     0, 1, 1, 1, 0},

    //número 3
    {0, 1, 1, 1, 0, 
     0, 0, 0, 1, 0, 
     0, 1, 1, 0, 0, 
     0, 0, 0, 1, 0, 
     0, 1, 1, 1, 0},

    //número 4  
    {0, 1, 0, 0, 0, 
     0, 0, 0, 1, 0, 
     0, 1, 1, 1, 0, 
     0, 1, 0, 1, 0, 
     0, 1, 0, 1, 0},

    //número 5
    {0, 1, 1, 1, 0, 
     0, 0, 0, 1, 0, 
     0, 1, 1, 1, 0, 
     0, 1, 0, 0, 0, 
     0, 1, 1, 1, 0},
    
    //número 6
    {0, 1, 1, 1, 0, 
     0, 1, 0, 1, 0, 
     0, 1, 1, 1, 0, 
     0, 1, 0, 0, 0, 
     0, 1, 1, 1, 0},
    
    //número 7
    {0, 1, 0, 0, 0, 
     0, 0, 0, 1, 0, 
     0, 1, 0, 0, 0, 
     0, 0, 0, 1, 0, 
     0, 1, 1, 1, 0},
    
    //número 8
    {0, 1, 1, 1, 0, 
     0, 1, 0, 1, 0, 
     0, 1, 1, 1, 0, 
     0, 1, 0, 1, 0, 
     0, 1, 1, 1, 0},
    
    //número 9
    {0, 1, 0, 0, 0, 
     0, 0, 0, 1, 0, 
     0, 1, 1, 1, 0, 
     0, 1, 0, 1, 0, 
     0, 1, 1, 1, 0}
};

static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void exibir_numero(bool *buffer, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = urgb_u32(r, g, b);

    for(int i = 0; i < NUM_PIXELS; i++)
    {
        put_pixel(buffer[i] ? color : 0);
    }
}

//rotina da interrupção
static void gpio_irq_button(uint gpio, uint32_t events)
{
    // Obtém o tempo atual em microssegundos
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    static volatile uint32_t last_time_0 = 0; // Armazena o tempo do último evento para button_0 (em microssegundos)
    static volatile uint32_t last_time_1 = 0; // Armazena o tempo do último evento para button_1 (em microssegundos)

    if(gpio == Button_0)
    {
        // Verifica se passou tempo suficiente desde o último evento
        if ((current_time - last_time_0) > Tempo_Delay) // 200 ms de debouncing
        {
            desenho_atual = (desenho_atual + 1) % 10; // Alterna para o próximo desenho 
            last_time_0 = current_time; // Atualiza o tempo do último evento
        }
    }
    else if(gpio == Button_1)
    {
        // Verifica se passou tempo suficiente desde o último evento
        if ((current_time - last_time_1) > Tempo_Delay) // 200 ms de debouncing
        {
            desenho_atual = (desenho_atual - 1 + 10) % 10; // Alterna para o próximo desenho 
            last_time_1 = current_time; // Atualiza o tempo do último evento
        }
    }
}

// Função de callback para piscar o LED
bool blink_led_callback(struct repeating_timer *t) {
    gpio_put(Led_R, !gpio_get(Led_R));
    return true;
}

// Função principal
int main()
{
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // Inicializa os pinos dos botões
    gpio_init(Button_0);
    gpio_set_dir(Button_0, GPIO_IN);
    gpio_pull_up(Button_0);

    gpio_init(Button_1);
    gpio_set_dir(Button_1, GPIO_IN);
    gpio_pull_up(Button_1);

    // Configura a interrupção para os botões
    gpio_set_irq_enabled_with_callback(Button_0, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_button);
    gpio_set_irq_enabled_with_callback(Button_1, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_button);

    // Inicializa o LED RGB
    gpio_init(Led_R);
    gpio_set_dir(Led_R, GPIO_OUT);

    // Configura o timer para piscar o LED
    struct repeating_timer timer;
    add_repeating_timer_ms(100, blink_led_callback, NULL, &timer);

    while (true)
    {
        exibir_numero(led_buffer[desenho_atual], 5, 0, 5);
        sleep_ms(100);
    }

    return 0;
}
