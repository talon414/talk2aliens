#include "pico/stdlib.h"  
#include "pico/bootrom.h"
#include <stdio.h>
#include <cstddef>
#include <cstring>
#include <string>
#include <tusb.h>
#include <math.h>         
#include <RF24.h>
#include "pico/binary_info.h"
#include "hardware/spi.h"
         
#define MISO 16
#define CS 17
#define SCLK 18
#define MOSI 19
#define CE 1

#define SPI_PORT spi0
#define SPI_SPEED 500000

void Spider(){
	spi_init(SPI_PORT,SPI_SPEED);
	
	gpio_set_function(MISO,GPIO_FUNC_SPI);
	gpio_set_function(SCLK,GPIO_FUNC_SPI);
	gpio_set_function(MOSI,GPIO_FUNC_SPI);
	gpio_set_function(CE,GPIO_FUNC_SPI);
	
	gpio_init(CE);
	gpio_set_dir(CE,GPIO_OUT);
	gpio_put(CE,1);
	
	gpio_init(CS);
	gpio_set_dir(CS,GPIO_OUT);
	gpio_put(CS,1);
}


#define BUFFER_SIZE 32 // nrf24l0 can handle max 32 bytes at a time

char msg[BUFFER_SIZE + 1];
uint8_t counter = 0;

void secret_data();

void led_stat(uint8_t state){
	const uint LED_PIN = PICO_DEFAULT_LED_PIN;
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	if(state==0){	//Tx
		gpio_put(LED_PIN, 1);
		sleep_ms(150);
		gpio_put(LED_PIN, 0);
		sleep_ms(150);
	}
	
	else {
		if(state==1){	//Rx
			gpio_put(LED_PIN, 1);
			sleep_ms(300);
			gpio_put(LED_PIN, 0);
			sleep_ms(300);
		}
		else{
			gpio_put(LED_PIN, 1);
			sleep_ms(500);
			gpio_put(LED_PIN, 0);
			sleep_ms(500);
		}
	}
}

// instantiate an object for the transceiver
RF24 radio(CE, CS);

bool rad_state;		// Rx=false ; Tx=true

bool rad_setup(){
	
	led_stat(2);

	bool radNum;
	
	msg[BUFFER_SIZE]=0;
	
	uint8_t address[][6]={"\xe1\xf0\xf0\xf0\xf0","\xd2\xf0\xf0\xf0\xf0"};
	
	
	while(!tud_cdc_connected()){
		sleep_ms(10);
	}
	
	if (!radio.begin()) {
		printf("radio hardware is not responding!!\n");
		return false;
    	}    	
    	
	
    	radio.setPALevel(RF24_PA_LOW);
	radio.setPayloadSize(BUFFER_SIZE);	//max 32 B.
	
	printf("'0' for Tx '1' for Rx\n\tEnter mode: ");
	char inp = getchar();
	rad_state = (bool)((int)inp==1);
	
	if (!rad_state) {
		radio.openWritingPipe(address[0]);
		radio.openReadingPipe(1,address[1]);
        	radio.stopListening(); // put radio in TX mode
        	printf("In Transmission\n");
    	}
    	else{
    		radio.openWritingPipe(address[1]);
		radio.openReadingPipe(1,address[0]);
       		radio.startListening(); // put radio in RX mode
       		printf("In Reception\n");
       		led_stat(1);
    	}
    	return true;
}

void black_hole(){
	if(!rad_state){		// Tx node
		
		radio.flush_tx();
		led_stat(0);
		uint8_t i=0,fail=0;
		uint64_t init_timer = to_us_since_boot(get_absolute_time()); // start the timer
		secret_data();
		while(1){
			if(radio.writeFast(&msg, BUFFER_SIZE)){
		        	i++;
		        	break;
		        }
		        else{
		        	fail++;	
		        	radio.reUseTX();
		        }
		}
		uint64_t end_timer = to_us_since_boot(get_absolute_time()); // end the timer
		
		printf("Time to transmit = %llu us with %d failures detected\n", end_timer - init_timer, fail);
		sleep_ms(1000);
		
	}
	
	else{ 		// Rx node
		led_stat(1);
		if (radio.available()){ //payload check
			radio.read(&msg, BUFFER_SIZE); // fetch payload from FIFO

			// print the received payload and its counter
			printf("Received: %s - %d\n", msg, counter++);
		}
    
	}
	
}

void secret_data(){
	while(1){
		printf("Enter text...\n");
		scanf ("%s", &msg); 
		if(sizeof(*msg<32)){
			break;	
		}
		printf("size is greater than 32 B\nRe enter message\n");
	}
}

int main(){

	stdio_init_all();
	
	Spider();
	
	while(!rad_setup()){
		printf("Initializing hardware please wait\n");
	}	
	
	printf("Initialized hardware\n");
	
	while(1){
		black_hole();
	}
	
	return 0;
}