#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hal_led.h"
#include "btstack.h"
#include "shutter.h" // Generated automatically from shutter.gatt by CMake

#define BUTTON_PIN                           6         // GPIO pin for the button (active low) Note on the RP PICO W I solder TP6 to GPIO6 for the [BOOT] button to work. Iamage capture
#define DB_BAUD_RATE                         115200
#define UART0_TX_PIN                         0


// Standard USB HID Report Descriptor for Consumer Control (Volume Up / Down)
static const uint8_t hid_descriptor_consumer[] = {
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1 bit)
    0x95, 0x02,        //   Report Count (2 bits)
    0x09, 0xE9,        //   Usage (Volume Increment) -> Bit 0
    0x09, 0xEA,        //   Usage (Volume Decrement) -> Bit 1
    0x81, 0x02,        //   Input (Data, Var, Abs)
    0x95, 0x06,        //   Report Count (6 bits padding)
    0x81, 0x03,        //   Input (Const, Var, Abs)
    0xC0               // End Collection
};

// BLE Advertising Data (Flags + Complete Name + Appearance: HID Generic)
static const uint8_t adv_data[] = {
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    0x0E, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'P','i','c','o','W',' ','S','h','u','t','t','e','r',
    0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xC0, 0x03,                                 // 0x03C0 = Generic HID
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x12, 0x18 // HID Service 0x1812
};

static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static uint8_t battery_level = 100;
static uint8_t report_to_send = 0;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static btstack_timer_source_t button_timer;

static void can_send_now(void *context);



static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
	if (packet_type != HCI_EVENT_PACKET) return;

	switch (hci_event_packet_get_type(packet)) {
	case HCI_EVENT_DISCONNECTION_COMPLETE:
		con_handle = HCI_CON_HANDLE_INVALID;
		gap_advertisements_enable(1);
		break;

	case SM_EVENT_JUST_WORKS_REQUEST:
		sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
		break;

	case HCI_EVENT_HIDS_META:
		switch (hci_event_hids_meta_get_subevent_code(packet)) {
		case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
			con_handle = hids_subevent_input_report_enable_get_con_handle(packet);
			printf("HIDS enabled, handle: 0x%04x\n", con_handle);
			break;

		case HIDS_SUBEVENT_CAN_SEND_NOW:
			if (con_handle != HCI_CON_HANDLE_INVALID) {
				hids_device_send_input_report(con_handle, &report_to_send, 1);
				uint8_t status = printf("Sent 0x%02x, status: 0x%02x\n", report_to_send, status);
				hal_led_toggle();
			}
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}
}


static void button_poll_handler(btstack_timer_source_t *ts) {
    static bool last_state = true;
    bool current_state = gpio_get(BUTTON_PIN); // Active low (false = pressed)

    if (current_state != last_state && con_handle != HCI_CON_HANDLE_INVALID) {
        last_state = current_state;
	    printf("Button changed: %d | con_handle: 0x%04x\n", current_state, con_handle);
        report_to_send = (current_state == false) ? 0x01 : 0x00;
	    printf("report_to_send %d \n", report_to_send);
        hids_device_request_can_send_now_event(con_handle);
    }

    // Re-arm timer every 20ms (handles polling + basic debouncing)
    btstack_run_loop_set_timer(ts, 20);
    btstack_run_loop_add_timer(ts);
}

static int led_state = 0;

void hal_led_toggle(void) {
	led_state = 1 - led_state;
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
}





int main() {
    stdio_init_all();
    stdio_uart_init_full(uart0, DB_BAUD_RATE, UART0_TX_PIN, -1);// Initialize all basic stdio functions
	printf("Starting! main()\n");
	
    if (cyw43_arch_init()) {
        return -1;
    }

    // Initialize GPIO button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

	
    // Initialize L2CAP and Security Manager (Bonding is required by iOS/Android for HID)
    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    // Initialize ATT Server and HID Device Service
    att_server_init(profile_data, NULL, NULL);
    battery_service_server_init(battery_level);
    device_information_service_server_init();
    hids_device_init(0, hid_descriptor_consumer, sizeof(hid_descriptor_consumer));

    // Register event callbacks
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    hids_device_register_packet_handler(packet_handler);

    // Configure BLE Advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, 0, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(sizeof(adv_data), (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // Start polling GPIO button
    button_timer.process = &button_poll_handler;
    btstack_run_loop_set_timer(&button_timer, 20);
    btstack_run_loop_add_timer(&button_timer);

    // Turn on Bluetooth and run
    hci_power_control(HCI_POWER_ON);
    btstack_run_loop_execute();
    return 0;
}