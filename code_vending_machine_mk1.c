/*
   Code created by Herr Technik on January 7th, 2026
   
   Please read the "Notes on the circuit diagram" file for a deeper understanding of the program and the circuit
   
   This code assumes the following:
      > You are using an active low relay board (activatePump function sends a zero to activate the corresponding pump, and 1's to deactivate the others)
      
   Follow me on my social media to receive the latest updates of my projects
      Youtube Channel: HerrTechnik
      https://www.youtube.com/@HerrTechnikMKI
   E-mail for business and sponsorships:
      contactotransistorizedmx@gmail.com
   Donations:
      https://buymeacoffee.com/herrtechnik
      https://paypal.me/HerrTechnik
*/

#include <16F887.h>
#fuses INTRC_IO, NOWDT, NOPROTECT, BROWNOUT    
#use delay(clock=4000000)
#use I2C(MASTER, SDA=PIN_C0, SCL=PIN_C1, FAST)
#define ADDRESS_LCD 0x4E
#include <i2c_Flex_LCD.c> // LCD library
#include <stdint.h> // integers library

// SETTINGS
#define ENGLISH // Define this if you want English, if not defined, Spanish will be used
#define STD_DELAY 1500// Standard delay
#define TINY_DELAY 50 // Tiny delay
#define PRODUCTS_NUMBER 10 // Number of products (varies according to the model)
#define ROWS_NUMBER 2 // Number of rows in the button matrix
#define COLUMNS_NUMBER 5 // Number of columns in the button matrix
#define LCD_DISPLAY_ADDRESS 0x4E // i2c address used to communicate with LCD display
#define LCD_DISPLAY_COLUMNS 16 // Number of columns in the LCD display
#define LCD_DISPLAY_ROWS 2 // Number of rows in the LCD display
#define NULL_CHAR 0x00 // Null character that is interpreted as "No button pressed"
#define DISABLE_PUMPS 16 // This value must be entered to activatePump() function to disable all operative pumps
#define SHIFT_REGISTER_OUTPUTS 16 // Number of outputs when the two 595 shift registers are combined

// MACROS
#define VALUE_IS_NOT_A_PRODUCT(k) ((k) < 'A' || (k) > 'J') // Opposite of VALUE_IS_A_PRODUCT
#define VALUE_IS_A_PRODUCT(k) ((k) >= 'A' && (k) <= 'J') // This macro checks if the value is between A and J inclusive
#define ASCII_TO_INDEX(k) ((k) - 'A') // This macro converts an ascii character into a zero-based index

// Define the names of the products
#ifdef ENGLISH
   #define PRODUCT1 "Lemon"
   #define PRODUCT2 "Grapes"
   #define PRODUCT3 "Orange"
   #define PRODUCT4 "Soda"
   #define PRODUCT5 "Beer"
   #define PRODUCT6 "Vodka"
   #define PRODUCT7 "Whisky"
   #define PRODUCT8 "Milk"
   #define PRODUCT9 "Water"
   #define PRODUCT10 "Pancho cola"
#else
   #define PRODUCT1 "Cloro"
   #define PRODUCT2 "Pinol verde"
   #define PRODUCT3 "Lavanda"
   #define PRODUCT4 "Citrico"
   #define PRODUCT5 "Pera manzana"
   #define PRODUCT6 "Mas color"
   #define PRODUCT7 "Ariel"
   #define PRODUCT8 "Flor de luna"
   #define PRODUCT9 "Axion"
   #define PRODUCT10 "Desengrasante"
#endif

// Pins definitions
   // Pins connected to the 2x5 button matrix
   #define ROW1 PIN_B0// Inputs They need weak pull up resistors (around 10k)
   #define ROW2 PIN_B1
   #define COL1 PIN_B2 // OUTPUTS
   #define COL2 PIN_B3 
   #define COL3 PIN_B4
   #define COL4 PIN_B5
   #define COL5 PIN_B6
   const int16 ROWS[ROWS_NUMBER] = {ROW1, ROW2};
   const int16 COLUMNS[COLUMNS_NUMBER] = {COL1, COL2, COL3, COL4, COL5};
   // Pins connected to 595 IC
   #define SER PIN_C5 // Pin that sends serial data to 595 IC
   #define LATCH PIN_B7 // Pin that enables internal register of 595 IC
   #define CLK PIN_C6 // Pin that sends clock pulses to shift register of 595 IC
   #define RELAY_ENABLE PIN_C3 // Pin that pulls low the output enable pin of both 595's after it's been charged with ones (to avoid burnign the fuse at start-up)
   // Pins connected to the two 7493 binary counters
   #define COUNTER_RESET PIN_C2 // Output used to reset both counters when the IC successfully reads the stored value in them
   #define COUNTER0 PIN_D0 // Inputs used to receive what is stored in the counters
   #define COUNTER1 PIN_D1
   #define COUNTER2 PIN_D2
   #define COUNTER3 PIN_D3
   #define COUNTER4 PIN_D4
   #define COUNTER5 PIN_D5 // Up to number 63
   // Pins connected to the especial function button and the mode switch
   #define MODE_SW PIN_C7 // This is connected to the dip switch (pull-down resistor)
   #define SPECIAL_BUTTON PIN_C4 // This is connected to an arcade button (pull-down resistor)
   
// Prototypes for custom functions  
   int8 scanKeypad(void); // Basic function used to analyze the keypad
   void keypad(void); // Advanced function used to keep track of previously pressed keys and decrease bouncing
   void activatePump(int val); // Function used to activate the required pump, only one at a time
   void stdDelay(void); // Function that contains the standard delay
   void tinyDelay(void); // Function that contains the tiny delay
   void printStringROM(int16 ptr); // This function prints a string from the ROM memory
   void clearScreen(void); // Clear screen
   void showNames(int16 namePtr); // Clears the screen and shows tha name of the selected product
   void showMessage(uint8_t message, int1 delay = 1, int1 clear = 1); // Displays the required message to the user
   
// EEPROM ADDRESSES
#define START_ADDRESS 0x00 // Address at which EEPROM use starts
#define PRICE_ADDRESS START_ADDRESS // 10 consecutive bytes to store the price of each product
#define SALES_ADDRESS PRICE_ADDRESS + PRODUCTS_NUMBER // 10 consecutive bytes to store the sales of each product
#define AVAILABILITY_ADDRESS SALES_ADDRESS + PRODUCTS_NUMBER // 10 consecutive bytes to store the availability of each product
#define TIMER_ADDRESS AVAILABILITY_ADDRESS + PRODUCTS_NUMBER // 10 consecutive bytes to store the time it takes for each corresponding pump to dispense one liter of that product 1 means 100 ms and 255 means 25.5 seconds
#define TOTAL_SALES_ADDRESS TIMER_ADDRESS + PRODUCTS_NUMBER // 4 consecutive bytes to store the total sales as a 4-byte unsigned integer

// Global variables
   // Variables used for display
   uint8_t key = NULL_CHAR;
   int16 productNames[PRODUCTS_NUMBER];
   // Variables used for money
   int8 productPrice[PRODUCTS_NUMBER]; // This array will be populated with the eeprom data (0x00 to 0x09)
   int8 productAvailable[PRODUCTS_NUMBER]; // Same but (0x14 to 0x1D) records whether a product is available or not
   int8 productTimer[PRODUCTS_NUMBER]; // This array will store the time it takes for a pump to dispense 1 L of the corresponding product
   // Variables used for processing
   uint8_t selectedProduct; // Variable used to store the selected product
   volatile unsigned int8 credit = 0; // Variable used to store the current credit of the user
   volatile unsigned int8 previousCredit = 0; // This variable is used to stabilize the reading process of the two binary counters and avoid errors
   volatile int1 moneyInserted = false; // Boolean variable used as a flag to indicate if the user has inserted money.
   
   // Configuration variables
   int8 matrix[ROWS_NUMBER][COLUMNS_NUMBER] = // rows x columns
   {
      {'J', 'H', 'F', 'D', 'B'},
      {'I', 'G', 'E', 'C', 'A'}
   };
   
// Function used to read the value stored in the two 7493 counters and add it to the credit variable
// Interrupt Service Routine
#INT_TIMER1
void readCounter(void) 
{
   if (input_d() != 0x00)
   {
      if (previousCredit == input_d())
      {
         credit += input_d() & 0b00111111; // And add that to the credit variable
         output_high(COUNTER_RESET); // Reset both counters 
         output_low(COUNTER_RESET);
         moneyInserted = true;
      }
      else
      {
         previousCredit = input_d();
      }
   }
}

void main(void)
{
   // Configuration
      // PORT A (UNUSED)
      set_tris_a(0b00000000);
      output_a(0b00000000);
      // PORT B(KEYPAD)
      set_tris_b(0b00000011); // Keypad's pins | rows are inputs and columns are outputs
      output_b(0b00000000); // MSB - LSB
      for (int8 i = 0; i < COLUMNS_NUMBER; i++)
      {
         output_float(COLUMNS[i]);
      }
      // PORT C (I2C, 595 AND 7493 CONTROL
      set_tris_c(0b00000000);
      output_c(0b00001000); // MSB - LSB
      // PORT D (INPUT FROM COUNTERS)
      set_tris_d(0b00111111);
      output_d(0b00000000); // MSB - LSB
      // Setup timer 1
      setup_timer_1(T1_INTERNAL | T1_DIV_BY_8);
      disable_interrupts(GLOBAL);
      // LCD
      lcd_init(LCD_DISPLAY_ADDRESS, LCD_DISPLAY_COLUMNS, LCD_DISPLAY_ROWS);
      // Initialize productNames array
      showMessage(0, 1, 1); // "Starting machine"
      productNames[0] = &PRODUCT1;
      productNames[1] = &PRODUCT2;
      productNames[2] = &PRODUCT3;
      productNames[3] = &PRODUCT4;
      productNames[4] = &PRODUCT5;
      productNames[5] = &PRODUCT6;
      productNames[6] = &PRODUCT7;
      productNames[7] = &PRODUCT8;
      productNames[8] = &PRODUCT9;
      productNames[9] = &PRODUCT10;

      // Populating product availability array, product price array and product timer array
      for (int8 i = 0; i < PRODUCTS_NUMBER; i++)
      {
         productPrice[i] = read_eeprom(PRICE_ADDRESS + i);
         productAvailable[i] = read_eeprom(AVAILABILITY_ADDRESS + i);
         productTimer[i] = read_eeprom(TIMER_ADDRESS + i);
      }
      output_high(COUNTER_RESET); // Reset both counters 
      output_low(COUNTER_RESET);
   
      // Deactivate all pumps and enable relays (active low)
      activatePump(DISABLE_PUMPS);
      output_low(RELAY_ENABLE);

   // Main program that is executed endlessly
   moneyInserted = true; // Turns on this flag regardless the past events.
   while (true)
   {
      // Waiting for user to select a valid product and insert enough coins
      key = NULL_CHAR;
      enable_interrupts(GLOBAL);
      enable_interrupts(INT_TIMER1);
      // This part cannot be done by chooseProduct() function because this includes more functionality
      while (VALUE_IS_NOT_A_PRODUCT(key))
      {    
         showMessage(1, 0, 1); // "Insert money"
         while (true)
         {
            keypad();
         }
         if (VALUE_IS_A_PRODUCT(key))
         {
            selectedProduct = ASCII_TO_INDEX(key);
            if (productAvailable[selectedProduct] == false) // If the product is not available
            {
               showMessage(3, 1, 1); // "Product not available"
               key = NULL_CHAR; // Avoid escaping loop
            }
            else if (productPrice[selectedProduct] > credit) // If the credits are insufficient to purchase the selected product
            {
               showMessage(4, 1, 1); // "Insufficient credit"
               key = NULL_CHAR; // Avoid escaping loop
            }
         }
      }
      // Once the user has selected an available product...
      credit -= productPrice[selectedProduct]; // Decrease the current number of credits by the price of the purchased product
      showNames(productNames[selectedProduct]);// Display the name of the selected product
      showMessage(5, 1, 0); // "selected"
      showMessage(6, 0, 1); // "Dispensing product"
      // Disable all interrupts to avoid any delay
      disable_interrupts(GLOBAL);
      // Activate the corresponding pump
      activatePump(selectedProduct + 1);
      // Individual calibrated time for each product
      for (uint8_t i = 0; i < productTimer[selectedProduct]; i++)
      {
         tinyDelay();
         tinyDelay();
      }
      // And after a certain time, turn off that pump
      activatePump(DISABLE_PUMPS);
      // Display a farewell message and restart the main program
      showMessage(7, 1, 1); // "Thanks for buying"
   }
} // end of main function

///////// Custom functions //////////
int8 scanKeypad(void) // STATUS: ACTIVE
{
   for (int8 row = 0; row < ROWS_NUMBER; row++)
   {
      for (int8 column = 0; column < COLUMNS_NUMBER; column++)
      {
         output_low(COLUMNS[column]);
         if (input(ROWS[row]) == 0)
         {
            output_float(COLUMNS[column]);
            return matrix[row][column];
         }
         output_float(COLUMNS[column]);
      }
   }
   return NULL_CHAR;
}

void keypad(void) // STATUS: ACTIVE
{
   while (true) // waits until user presses a key
   {
      if (key != scanKeypad())
      {
         tinyDelay();
         key = scanKeypad();
         if (key != NULL_CHAR)
         {
            return;
         }
      }
      if (moneyInserted) // If user has inserted money...
      {
         moneyInserted = false; // Clear that flag
         lcd_gotoxy(1, 2); // And display the current credit on the screen
         printf(lcd_putc, "$%u  ", credit);
      }
   }
}

void activatePump(int val) // STATUS: ACTIVE
{
   if (val < 1 || val > SHIFT_REGISTER_OUTPUTS) // Reject if value is unvalid
      return;
   for (int i = SHIFT_REGISTER_OUTPUTS; i > 0; i--) // For every bit...
   {
      if (i == val) // If this bit should be on...
      {
         output_low(SER); // Send 0 (active low)
      }
      else
      {
         output_high(SER); // Otherwise, send 1 (active low)
      }
      output_low(CLK); // Shift the bit
      output_high(CLK);
   }
   output_low(LATCH); // And charge them to the outputs at the end
   output_high(LATCH);
}

void stdDelay(void) // STATUS: ACTIVE
{
   delay_ms(STD_DELAY); // Avoid inlining
}

void tinyDelay(void) // STATUS: ACTIVE
{
   delay_ms(TINY_DELAY); // Avoid inlining
}

void printStringROM(int16 ptr) // STATUS: ACTIVE
{  
   uint8_t temp; //
   while (true)
   {
      read_program_memory(ptr++, &temp, 1); // Read program memory byte by byte 
      if (temp == 0x00) // If it's a null character (end of string), return
      {
         return;
      }
      lcd_putc(temp); // Otherwise print that character
   }
}

void clearScreen(void) // STATUS: ACTIVE
{
   lcd_putc("\f");
}

void showNames(int16 namePtr) // STATUS: ACTIVE
{
   clearScreen();
   printStringROM(namePtr); // Shows name
}

void showMessage(uint8_t message, int1 delay = 1, int1 clear = 1) // STATUS: ACTIVE
{
   if (clear)
   {
      clearScreen(); // Clear screen before printing the message
   }
   switch (message)
   {
      case 0:
      #ifdef ENGLISH
         lcd_putc("Starting\nmachine");
      #else
         lcd_putc("Iniciando\nmaquina"); 
      #endif
         break;
      case 1:
      #ifdef ENGLISH
      printf(lcd_putc, "Insert coins:\n$%u", credit);
      #else
      printf(lcd_putc, "Inserte dinero:\n$%u", credit);
      #endif
         break;
      case 3:
      #ifdef ENGLISH
      lcd_putc("Product not\navailable"); 
      #else
      lcd_putc("Producto no\ndisponible"); 
      #endif
         break;
      case 4:
      #ifdef ENGLISH
      lcd_putc("Insufficient\ncredit");
      #else
      lcd_putc("Credito\ninsuficiente");
      #endif
         break;
      case 5:
      #ifdef ENGLISH
      lcd_putc("\nselected");
      #else
      lcd_putc("\nseleccionado"); 
      #endif
         break;
      case 6:
      #ifdef ENGLISH
      lcd_putc("Dispensing\nproduct");
      #else
      lcd_putc("Llenando\nproducto");
      #endif
         break;
      case 7:
      #ifdef ENGLISH
      lcd_putc("Thanks for\nbuying");
      #else
      lcd_putc("Vuelva\npronto");
      #endif
         break;
      case 20:
      #ifdef ENGLISH
      lcd_putc("\nchosen");
      #else
      lcd_putc("\nelegido");
      #endif
         break;
   }
   if (delay)
   {
      stdDelay(); // Wait
   }
}

