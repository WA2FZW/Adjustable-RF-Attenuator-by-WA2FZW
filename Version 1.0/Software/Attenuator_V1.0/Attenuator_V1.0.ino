/*
 *	Attenuator Version 1.0 by John Price (WA2FZW)
 *
 *		This software runs on an Arduino Nano and is meant to control a pair of
 *		PE4312 auutenuator chips in series with each other. The PE4312 is capable
 *		of providing RF attenuation from 0dB to 31.5dB in half dB steps. By placing
 *		2 of them in series, a total of 63dB of attenuation is possible.
 */

#include <Arduino.h>						// General Arduino definitions
#include <Rotary.h>							// From https://github.com/brianlow/Rotary
#include <SPI.h>							// Needed for the display
#include <Wire.h>							// SPI bus controller - Needed or not?
#include <Adafruit_GFX.h>					// Common Adafruit display control
#include <Adafruit_SSD1306.h>				// For the 128 x 32 OLED
#include <PE43xx.h>							// Library for the 2 attenuator chips


/*
 *	Define the GPIO pins:
 *
 *		With the exception of the ENC_A and ENC_B pins and the SCA and SCL pins, the
 *		GPIO pin assignments were selected to facilitate the PCB layout. The pin names
 *		match the NET names on the PCB schematic.
 */

#define	ENC_SW		A0					// Encoder switch
#define	SDA			A4					// Display data line
#define	SCL			A5					// Display clock
#define	ENC_B		 2					// Encoder 'B'
#define	ENC_A		 3					// Encoder 'A'
#define	ATT2_LE		 4					// Attenuator #2 latch enable
#define	ATT2_CLK	 5					// Attenuator #2 clock
#define	ATT2_DATA	 6					// Attenuator #2 data
#define	ATT1_LE		 7					// Attenuator #1 latch enable
#define	ATT1_CLK	 8					// Attenuator #1 clock
#define	ATT1_DATA	 9					// Attenuator #1 data
#define	LED			13					// Running indicator LED

#define	LED_ON		HIGH				// What to do on the 'LED' pin to
#define LED_OFF		LOW					// turn it on or off


/*
 *	The values stored in the 'attenuation; variable are the actual attenuation times
 *	10, thus 'ENC_ADJUST' specifies that the value is adjusted by 5 for each encoder
 *	click. The other two definitions here define the range limits.
 */

#define	ENC_ADJUST			  5			// Adjust 'attenation' by this for each click
#define	MAX_ATTENUATION		630			// Maximum 'attenuation' times 10
#define	MIN_ATTENUATION		  0			// Minimum attenuation


/*
 *	'ATT_INIT' specifies the 'attenuation' setting when the program starts or when the
 *	encoder button is pressed.
 *
 *	'ATT_OFFSET' provides an ability to calibrate the attenuator. Previous tests on the
 *	PE4302 (predecessor to the PE4312) have shown that there is some attenuation even when
 *	the chip was set to zero. That attenuation might also vary with frequency. By assigning
 *	a value (x 10) to 'ATT_OFFSET', the adjustment will be added to the displayed
 *	attenuation; the actual value of 'attenuation' is maintained within the limits set
 *	above as that is the value that needs to be fed to the PE4312 chips.
 */

#define	ATT_INIT	 0					// Initial attenuation
#define	ATT_OFFSET	 0					// Calibration adjustment


/*
 *	These define the display parameters:
 */

#define DISP_WIDTH		 128				// OLED display width, in pixels
#define DISP_HEIGHT		  32				// OLED display height, in pixels
#define DISP_RST		  -1				// Reset pin is connected to the Arduino reset pin)
#define DISP_ADDR		0x3C				// 0x3C for the 128x32 display


/*
 *	Create the display object:
 */

	Adafruit_SSD1306 Display ( DISP_WIDTH, DISP_HEIGHT, &Wire, DISP_RST );


/*
 *	Create the rotary encoder object 
 */

	Rotary Encoder = Rotary ( ENC_A, ENC_B );


/*
 *	Create the two attenuator objects:
 */

	PE43xx att_1 ( ATT1_LE, ATT1_DATA, ATT1_CLK, PE4312 );
	PE43xx att_2 ( ATT2_LE, ATT2_DATA, ATT2_CLK, PE4312 );


/*
 *	Some global variables:
 */

int16_t		attenuation;					// Current attenuator setting
uint8_t		attChanged = false;				// Indicates 'attenuation' has changed


/*
 *	"setup" initializes all the GPIO pins and the serial port. The serial port isn't
 *	really used except for debugging.
 */

void setup()
{
	Serial.begin ( 115200 );						// Start the serial monitor port

/*
 *	Setup the GPIO pins. Note that we don't specify a mode for 'SDA' and 'SCL'. For some
 *	reason, when you do that, the display just shows nonsense!
 */

	pinMode ( ENC_A, INPUT_PULLUP );				// Encoder 'A'
	pinMode ( ENC_B, INPUT_PULLUP );				// Encoder 'B'
	pinMode ( ENC_SW, INPUT_PULLUP );				// Encoder Switch
	pinMode ( ATT2_LE, OUTPUT );					// Attenuator #2 latch enable
	pinMode ( ATT2_CLK, OUTPUT );					// Attenuator #2 clock
	pinMode ( ATT2_DATA, OUTPUT );					// Attenuator #2 data
	pinMode ( ATT1_LE, OUTPUT );					// Attenuator #1 latch enable
	pinMode ( ATT1_CLK, OUTPUT );					// Attenuator #1 clock
	pinMode ( ATT1_DATA, OUTPUT );					// Attenuator #1 data
	pinMode ( LED, OUTPUT );						// Unit is running indicator LED


/*
 *	Start the display. The 'SSD1306_SWITCHCAPVCC' setting tells the display to
 *	generate its 3.3V from the internal regulator as we're powering it from a
 *	5 volt source. If it doesn't start correctly, we send an error message to
 *	the serial monitor and flash the LED forever.
 */

	delay ( 100 );									// Without this, display doesn't start

	if( !Display.begin ( SSD1306_SWITCHCAPVCC, DISP_ADDR ))
	{
		Serial.println ( "SSD1306 allocation failed" );
		while ( true )
			BlinkLED ( 100, 5 );
	}


/*
 *	Initialize the attenuators. Note that the 'begin' function sets the attenuation
 *	to zero on the chips, but we will immediately set them to 'ATT_INIT'.
 */

	att_1.begin();
	att_2.begin();

	delay ( 100 );
	
	Serial.print ( "Att #1 - Steps = " );
	Serial.println ( att_1.getStep () );
	Serial.print ( "Att #2 - Steps = " );
	Serial.println ( att_2.getStep () );

	delay ( 100 );
	
	Serial.print ( "Att #1 - Max = " );
	Serial.println ( att_1.getMax () );
	Serial.print ( "Att #2 - Max = " );
	Serial.println ( att_2.getMax () );

	attenuation = ATT_INIT;							// Set initial 'attenuation' value
	SetAttenuation ();								// Feed it to the chips
	ShowAttenuation ();								// And display it

	digitalWrite ( LED, LED_ON );					// Turn on the alive indicator

	Serial.println ( "WA2FZW 63dB Attenuator - Version 1.0" );	// And announce on the serial monitor


/*
 *	Set up the interrupts for the rotary encoder. Some documents online would lead one to
 *	believe that the encoder can be handled without interrupts, but I couldn't make it work!
 *	If the encoder seems to work backwards, simply flip the pin number assignments above.
 */

	attachInterrupt ( digitalPinToInterrupt ( ENC_A ), Read_Encoder, CHANGE );
	attachInterrupt ( digitalPinToInterrupt ( ENC_B ), Read_Encoder, CHANGE );

}													// End of 'setup'


void loop ()
{
	if ( attChanged )								// If the encoder was moved
	{
		attChanged = false;							// Clear the changed flag
		SetAttenuation ();							// Tell the chips
		ShowAttenuation ();							// Tell the operator
	}

	if ( !digitalRead ( ENC_SW ))					// If the encoder switch is active
	{
		while ( !digitalRead ( ENC_SW )) {}			// Wait for it to be released

		attenuation = ATT_INIT;						// Set the initial 'attemuation'
		attChanged = true;							// and indicate it changed
	}
}


/*
 *	The Read_Encoder function is called via the Arduino's interrupt mechanism, and
 *	never invoked by any other means. based on which way the encoder moved, we 
 *	either increment or decremnt the 'attenuation' by 0.5dB per click.
 *
 *	But note, 'attenuation' is maintained as an interger value that is the desired
 *	'attenuation' times 10, so we increment or decrement by '5' ('ENC_ADJUST').
 */

void Read_Encoder ()
{

/*
 *	Read the encoder. The Rotary object actually does the reading of the state of
 *	the encoder pins and determines which way it is moving. If the knob is being
 *	turned counter-clockwise, we decrement the 'attenuation'. If the knob is being
 *	turned clockwise, we increment 'attenuation.
 *
 */

	unsigned char result = Encoder.process ();

	if ( result == DIR_NONE )					// Encoder didn't move (didn't think this
		return;									// was actually possible, but it is!)

	else if ( result == DIR_CW )				// Encoder rotated clockwise
		attenuation += ENC_ADJUST;				// So increment the 'attenuation'

	else if ( result == DIR_CCW )				// Encoder rotated counter-clockwise
		attenuation -= ENC_ADJUST;				// So increment the 'attenuation'

	if ( attenuation > MAX_ATTENUATION )		// Limit checks
		attenuation  = MAX_ATTENUATION;

	if ( attenuation < MIN_ATTENUATION )
		attenuation  = MIN_ATTENUATION;

	attChanged = true;							// Signal display & chip updates needed
}


/*
 *	'SetAttenuation' tells the chips what to do. 'att_1' handles 'attenuation' values
 *	from 0 to 31.5 dB on its own ('att_2' gets set to zero).
 *
 *	For values of 'attenuation' greater than 31.5dB, 'att_1' gets set for 31.5dB and
 *	'att_2' handles the rest.
 *
 *	Note that the 'ATT_OFFSET' does NOT come into play here; that only applies to the
 *	displayed value.
 */

void SetAttenuation ( void )
{
uint16_t	att1Setting;						// Setting for 'att_1'
uint16_t	att2Setting;						// Setting for 'att_2'

float		att1Level;							// Apparantely the library expects
float		att2Level;							// Floating point numbers

	if ( attenuation <= 315 )					// Only need att_1 active
	{
		att1Setting = attenuation;
		att2Setting = 0;
	}

	else										// Need both active
	{
		att1Setting = 315;
		att2Setting = attenuation - 315;
	}


/*
 *	Calculate the numbers the library needs to feed to the attenuator chips and
 *	set them:
 */

	att1Level = (float) att1Setting / 10;				// Remember we keep the 'attenuation'
	att2Level = (float) att2Setting / 10;				// times 10

	att_1.setLevel ( att1Level );				// Tell the chips
	att_2.setLevel ( att2Level );

	Serial.print ( "Set Att #1 level = " );
	Serial.println ( att1Level );
	Serial.print ( "Set Att #2 level = " );
	Serial.println ( att2Level );
}


/*
 *	'ShowAttenuation' puts the current value of 'attenuation' on the display.
 *
 *	The actual 'attenuation' is stored as an integer which is 10x the actual value
 *	which saves having to do floating point math. But we have to do a little work
 *	to isolate each digit before we can update the display.
 *
 *	The 'ATT_OFFSET' is applied to the displayed 'attenuation'.
 */


void ShowAttenuation ( void )
{
char	dispBuffer[8];						// We need 7 characters displayed

int16_t	digit1;								// 10s digit (or maybe blank)
int8_t	digit2;								// Units digit
int8_t	digit3;								// After the decimal point

int8_t	temp;								// Temporary number
int16_t	adjustedAttenuation;				// Local copy with 'ATT_OFFSET' applied
char	digits[] = "0123456789";			// Character lookup array

	adjustedAttenuation = attenuation + ATT_OFFSET;
	
	digit3 = adjustedAttenuation % 10;		// Get the decimal place digit
	temp   = adjustedAttenuation / 10;		// and strip it off the full value
	digit2 = temp % 10;						// Get the units digit
	digit1 = temp / 10;						// And strip it leaving just the 10s digit


/*
 *	Now the isolated digits are used as indicies into the 'digits' array to get
 *	the ASCII characters to be displayed. I might just change this to do a 
 *	numerical conversion from integer to ASCII eventually.
 */

	if ( digit1 )								// If the 10s digit is non-zero
		dispBuffer[0] = digits[digit1];			// Show it
	else										// Otherwise
		dispBuffer[0] = ' ';					// show a blank

	dispBuffer[1] = digits[digit2];				// Units
	dispBuffer[2] = '.';						// Decimal point
	dispBuffer[3] = digits[digit3];				// Decimal digit
	dispBuffer[4] = ' ';
	dispBuffer[5] = 'd';
	dispBuffer[6] = 'B';
	dispBuffer[7] = 0;							// Null string terminator

	Display.clearDisplay ();				

	Display.setTextSize ( 3 );					// Draw 3x scale text
	Display.setTextColor ( SSD1306_WHITE );		// Only color available!
	Display.setCursor ( 2, 10 );				// Top left corner of the text
	Display.print ( dispBuffer );				// Sets up the display's buffer
	Display.display ();							// Actually puts it on the screen

	Serial.print ( "Read level - Att #1 = " );
	Serial.println ( att_1.getLevel () );
	Serial.print ( "Read level - Att #2 = " );
	Serial.println ( att_2.getLevel () );
}


void BlinkLED ( uint32_t rate, uint16_t count )
{
	for ( int i = 0; i < count; i++ )
	{
    	digitalWrite ( LED, LED_OFF );
		delay ( rate );
   		digitalWrite ( LED, LED_ON );
		delay ( rate );
	}
}
