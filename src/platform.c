#include "platform.h"
#include "platform-private.h"

#include "spi.h"
#include "display.h"
#include "usb-midi.h"

usbd_device *usbd_dev;

static volatile uint32_t jiffies = 0;

static void platform_init_clock(void)
{
    rcc_clock_setup_hse_3v3(&rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_120MHZ]);

    rcc_periph_clock_enable(RCC_SPI1);
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
}

static void platform_init_gpio_usb(void)
{
    gpio_mode_setup(
        GPIOA,
        GPIO_MODE_AF,
        GPIO_PUPD_PULLUP,
        GPIO9 | GPIO11 | GPIO12
    );

    gpio_set_af(
        GPIOA,
        GPIO_AF10,
        GPIO9 | GPIO11 | GPIO12
    );
}

static void platform_init_gpio_spi(void)
{
    gpio_mode_setup(
        GPIOA,
        GPIO_MODE_OUTPUT,
        GPIO_PUPD_PULLUP,
        DISP_CS1
    );

    gpio_mode_setup(
        GPIOB,
        GPIO_MODE_OUTPUT,
        GPIO_PUPD_PULLUP,
        (
            SPI_DISP_RESET_PIN |
            SPI_DISP_DC_PIN |
            DISP_CS_PORTB
        )
    );

    gpio_mode_setup(
        GPIOA,
        GPIO_MODE_AF,
        GPIO_PUPD_PULLUP,
        SPI1_MOSI_PIN | SPI1_MISO_PIN | SPI1_CLOCK_PIN
    );

    gpio_set_af(
        GPIOA,
        SPI1_PIN_AF,
        SPI1_MOSI_PIN | SPI1_MISO_PIN | SPI1_CLOCK_PIN
    );
}

static void platform_init_usb(void)
{
    usbd_dev = usbd_init(USBD_STM32_OTG_FS, NULL, &usb_midi_device_info);
    usbd_register_set_config_callback(usbd_dev, usb_midi_set_config);
}

static void platform_init_display(void)
{
    gpio_clear(GPIOA, DISP_CS_PORTA);
    gpio_clear(GPIOB, DISP_CS_PORTB);

    spi_init(0, 0);
    display_transport_reset();
    display_init();
}

void sys_tick_handler()
{
    jiffies++;
}

uint32_t platform_jiffies(void)
{
    return jiffies;
}

static void platform_init_systick(void)
{
    /* clock rate / 1000 to get 1mS interrupt rate */
    systick_set_reload(168000);
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_counter_enable();
    systick_interrupt_enable();
}

void platform_init(void)
{
    platform_init_clock();
    platform_init_gpio_usb();
    platform_init_gpio_spi();
    platform_init_display();
    platform_init_usb();
    platform_init_systick();
}

void platform_poll(void)
{
    usbd_poll(usbd_dev, 0);
}

