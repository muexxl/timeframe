//
// Author Stephan Muekusch
// stephan.muekusch@gmail.com
// 2020
//
//
//
//



//define Output Pins

#define MAG 12
#define PORTB_MAG 4 // Arduino PIN No minus 8

#define RED 3 // Pin of red led string 
#define GREEN 5 // Pin of green led string 
#define BLUE 2 // Pin of green led string 

#define LED_CYCLE 20 // update cycle of LED state below 20 does not work

// Analog PINs for reading the led brightness for color mixing
#define RED_INPUT A1 
#define GREEN_INPUT A3
#define BLUE_INPUT A4

// Analog PINS for reading the oszillation parameters
#define FREQ_INPUT A5 //Pin for adjusting the frequency of the magnet for tuning into resonancy
#define POWER_INPUT A7 //Pin for adjusting the power of the magnet
#define CYCLE_INPUT A6 //Pin for adjusting the speed of phase shifting led vs magnet

#define MAX_MAG_DUTY 12  // over 20 burns your IRF540N for sure. 
#define LED_FREQ_CYCLE 100 //cycle for speed reversal interval


const double pi =  4.0 * atan (1.0);

int half_frequency = 0; // 1 = half frequency of the magnet by switching on the magnet only every second interrupt cycle

double mag_frequency_adjust = 0.0 ; 
double real_mag_frequency;

int mag_cycle = 255;
float mag_duty = 5/100.0 ; // Value in %

double led_frequency;
double led_frequency_offset;

double red_duty = 1/100.0 ;
double green_duty = 1/100.0 ;
double blue_duty = 1/100.0 ;

unsigned int red_stop = 0;
unsigned int green_stop = 0;
unsigned int blue_stop = 0;

uint32_t microseconds = 0;

bool mag_active = 0; // only relevant if half_frequency = 1

unsigned int  newOCR1A, newOCR1B, newOCR2A, newOCR2B;


void setup(){

  cli();//stop interrupts
  
  pinMode(MAG, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);

  pinMode(RED_INPUT, INPUT);
  pinMode(GREEN_INPUT, INPUT);
  pinMode(BLUE_INPUT, INPUT);
  pinMode(FREQ_INPUT, INPUT);
  pinMode(POWER_INPUT, INPUT);
  pinMode(CYCLE_INPUT, INPUT);

// general intstructions related to ATMEL 328P timers

// https://sites.google.com/site/qeewiki/books/avr-guide/timers-on-the-atmega328

//set timer0 for controlling the lights
  TCCR0A = 0;// set entire TCCR0A register to 0
  TCCR0B = 0;// same for TCCR0B
  TIMSK0 = 0;
  TCNT0  = 0;//initialize counter value to 0
  
  OCR0A = LED_CYCLE;
  
  // turn on CTC mode
  TCCR0A |= (1 << WGM01);
  
  TCCR0B |= (1 << CS01) ;// set prescaler at 8
  
  // enable timer compare interrupt A
  TIMSK0 = (1 << OCIE0A);


//set timer2 for Magnet
  TCCR2A = 0;// set entire TCCR0A register to 0
  TCCR2B = 0;// same for TCCR0B
  TIMSK2 = 0;
  TCNT2  = 0;//initialize counter value to 0

  OCR2A = mag_cycle;
  OCR2B = round(OCR2A * (1-mag_duty)); //Switch on on interrupt B
  
  // turn on CTC mode
  TCCR2A |= (1 << WGM21);
  //set prescaler  
  TCCR2B |= (1 << CS22) | (1 << CS21)| (1 << CS20);     
  // enable timer compare interrupt
  TIMSK2 = (1 << OCIE2A) | (1 << OCIE2B);

//set timer1 for LED
  real_mag_frequency = 16e6 / 1024 / (OCR2A+1);
  led_frequency = real_mag_frequency ;

  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0,
  TIFR1 = 255; // clear pending interrupts
  TIMSK1 = 0;
  // set compare match register for 1hz increments
  OCR1A = round(16e6 / (led_frequency * 8))-1;//(16*10^6) / (1*1024) - 1 (must be <65536)
  //OCR1B = 2000 ;// one millisecond at prescaler = 8
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set prescaler to 8
  TCCR1B |= (1 << CS11) ;
  // enable timer compare interrupt A 
  TIMSK1 |= (1 << OCIE1A);// 
  //TIMSK1 |= (1 << OCIE1B);// enable ISR B
  update_parameters();
sei();//allow interrupts

}//end setup

//Interrupt routing for magnet via Timer 2 

ISR(TIMER2_COMPA_vect){  
    PORTB = 0; //switch magnet off

    //update registers
    OCR2A = newOCR2A;
    OCR2B = newOCR2B;
}

ISR(TIMER2_COMPB_vect){  

    PORTB = mag_active << PORTB_MAG; //switch magnet on if mag_active is true
    
    if (half_frequency){
        mag_active = !mag_active;
    } else {
      mag_active = true;
    }
}



//Interrupt routine for LEDs via Timer 2 

ISR(TIMER1_COMPA_vect){ 
   //lights_on();
   OCR1A = newOCR1A;    
}

ISR(TIMER1_COMPB_vect){  
  //nothing to do
}

//Interrupt routine for LEDs via Timer 1
//Interrupt routine for having microseconds available
 
ISR(TIMER0_COMPA_vect){  
  unsigned int timer_value = TCNT1;
  PORTD= (timer_value < red_stop) << RED |(timer_value < green_stop) << GREEN |(timer_value < blue_stop) << BLUE;
  microseconds += 10 ;
}

void update_parameters(){
    mag_cycle= round( 470 - analogRead(FREQ_INPUT) / 1023.0 * 330.0); 
    mag_duty = analogRead(POWER_INPUT)/102300.0 * MAX_MAG_DUTY + 0.01; // reading 0..1023 scaled to adjust 0.01 ..0.15
    green_duty= analogRead(GREEN_INPUT)/10230.0; // reading 0..1023 scaled to adjust 0.01 .. 0.11
    blue_duty= analogRead(BLUE_INPUT)/10230.0; // reading 0..1023 scaled to adjust 0.01 .. 0.11
    red_duty= analogRead(RED_INPUT)/10230.0; // reading 0..1023 scaled to adjust 0.0. .. 0.11

    // PHASE shifting of LED vs MAG 
    led_frequency_offset = pow(analogRead(CYCLE_INPUT)/1023.0, 2) * 2; // reading 0..1023 scaled to adjust  0.0 .. 1

    // adjustment of phase shift over time
    // this makes the time frame look much more interesting
    led_frequency_offset *= sin(2 * pi * microseconds * 1e-6 / LED_FREQ_CYCLE);
    
    //adjust timer2 register for magnet 
    if (mag_cycle < 256) {
      newOCR2A = mag_cycle;
      newOCR2B = round(newOCR2A * (1-mag_duty));
      half_frequency = 0;
    }
    else {
      // enable virtual extension of the 8bit timer to allow for frequencies lower than 61 Hz
      newOCR2A = mag_cycle / 2;
      newOCR2B = round(newOCR2A * (1 - mag_duty * 2));
      half_frequency = 1;
    }
     
    //adjust timer1 register for LED
    real_mag_frequency = 16e6 / 1024 / (newOCR2A+1)/ (half_frequency + 1 );
    led_frequency = real_mag_frequency - led_frequency_offset;

    newOCR1A = min(round(16e6 / (led_frequency * 8  ))-1, 65536);//(16*10^6) / (1*1024) - 1 (must be <65536)

    red_stop = round(newOCR1A* (red_duty));
    green_stop =round(newOCR1A* (green_duty) );
    blue_stop = round(newOCR1A* (blue_duty) );
}

void loop() {
  if (microseconds % 1000 == 0){
    update_parameters();
  }

}
