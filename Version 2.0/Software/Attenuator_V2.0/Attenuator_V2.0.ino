/*
 *	RF Attenuator - Version 2.0 by John Price (WA2FZW)
 *
 *		Version 1.0 of this program was designed to control a pair of PE4312 attenuator
 *		chips in series with each other which would have provided up to 63dB of
 *		attenuation in 1/2dB steps.
 *
 *		Unfortunately, I was not able to build the Version 1.0 hardware. I just could
 *		not get the (very small) SMD PE4312 chips soldered correctly on the PCB.
 *
 *		Plan 'B', therefore is to use one of the PE4302 modules available on eBay and
 *		settle for only 31.5 dB of attenuation.
 *
 *		Where the PE4312s in the original version were controlled via their serial
 *		interface, the pre-built modules only support the parallel operation.
 */

/*
 *	We need these Arduino libraries. The first 3 are built intothe Arduino IDE. The
 *	Adafruit libraries can be installed using the 'Manage libraries...' option under
 *	the 'Tools' menu in the IDE.
 *
 *	The 'Rotary' library can be obtained from the Github link provided.
 */

#include <Arduino.h>						// General Arduino definitions
#include <SPI.h>							// Needed for the display
#include <Wire.h>							// SPI bus controller - Needed or not?
#include <Adafruit_GFX.h>					// Common Adafruit display control
#include <Adafruit_SSD1306.h>				// For the 128 x 32 OLED
#include <Rotary.h>							// From https://github.com/brianlow/Rotary


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
#define	V1			 4					// 0.5 dB
#define	V2			 5					// 1.0 dB
#define	V3			 6					// 2.0 dB
#define	V4			 7					// 4.0 dB
#define	V5			 8					// 8.0 dB
#define	V6			 9					// 16.0 dB
#define	LED			13					// Running indicator LED

#define	LED_ON		HIGH				// What to do on the 'LED' pin to
#define LED_OFF		LOW					// turn it on or off

#define DEBUG		false				// When set to 'true' you get status messages


/*
 *	The value stored in the 'attenuation' variable is the actual attenuation times
 *	10, thus 'ENC_ADJUST' specifies that the value is adjusted by 5 for each encoder
 *	click. The other two definitions here define the range limits times 10.
 */

#define	ENC_ADJUST			  5			// Adjust 'attenation' by this for each click
#define	MAX_ATTENUATION		315			// Maximum 'attenuation' times 10
#define	MIN_ATTENUATION		  0			// Minimum attenuation


/*
 *	'ATT_INIT' specifies the 'attenuation' setting when the program starts or when the
 *	encoder push button is pressed.
 *
 *	'ATT_OFFSET' provides an ability to calibrate the attenuator. Previous tests on the
 *	PE4302 (and the datasheet) indicate that there is some attenuation even when the chip
 *	was set to zero.
 *
 *	That attenuation  also varies with frequency. By assigning a value (x10) to
 *	'ATT_OFFSET', the adjustment will be added to the displayed attenuation; the actual
 *	value of 'attenuation' is maintained within the limits set above as that is the value
 *	that needs to be fed to the PE4302 chip.
 *
 *	Note that even though the actual attenuator module works in half dB steps, the
 *	'ATT_OFFSET' can be any single decimal place value, for example, if the desired offset
 *	is 1.2 dB, assign a value of '12' to 'ATT_OFFSET'. The value is only applied to the
 *	display; not to the value fed to the module.
 *
 *	The calibration value might also depend on frequency; the documentation explains
 *	how to determine the appropriate value.
 */

#define	ATT_INIT	 0						// Initial attenuation
#define	ATT_OFFSET	12						// Calibration adjustment


/*
 *	These define the display parameters:
 */

#define DISP_WIDTH		 128				// OLED display width, in pixels
#define DISP_HEIGHT		  32				// OLED display height, in pixels
#define DISP_RST		  -1				// Reset pin is connected to the Arduino reset pin
#define DISP_ADDR		0x3C				// 0x3C is the I2C address for the display


/*
 *	Create the display object:
 */

	Adafruit_SSD1306 Display ( DISP_WIDTH, DISP_HEIGHT, &Wire, DISP_RST );


/*
 *	Create the rotary encoder object 
 */

	Rotary Encoder = Rotary ( ENC_A, ENC_B );


/*
 *	Some global variables:
 */

int16_t		attenuation;					// Current attenuator setting
bool		attChanged = false;				// Indicates 'attenuation' has changed


/*
 *	"setup" initializes all the GPIO pins and the serial port. The serial port isn't
 *	really used now except for debugging, although perhaps I will add the ability to set
 *	an attenuation via the serial interface.
 */

void setup()
{
	Serial.begin ( 115200 );						// Start the serial monitor port


/*
 *	Setup the GPIO pins. Note that we don't specify a mode for the 'SDA' and 'SCL' pins.
 *	For some reason, when you do that, the display just shows nonsense!
 */

	pinMode ( ENC_A, INPUT_PULLUP );				// Encoder 'A'
	pinMode ( ENC_B, INPUT_PULLUP );				// Encoder 'B'
	pinMode ( ENC_SW, INPUT_PULLUP );				// Encoder Switch
	pinMode ( V1, OUTPUT );							// 0.5 dB when HIGH
	pinMode ( V2, OUTPUT );							// 1.0 dB
	pinMode ( V3, OUTPUT );							// 2.0 dB
	pinMode ( V4, OUTPUT );							// 4.0 dB
	pinMode ( V5, OUTPUT );							// 8.0 dB
	pinMode ( V6, OUTPUT );							// 16 dB
	pinMode ( LED, OUTPUT );						// Unit is running indicator LED


/*
 *	Start the display. The 'SSD1306_SWITCHCAPVCC' setting tells the display to
 *	generate its 3.3V from the internal regulator as we're powering it from a
 *	5 volt source. If it doesn't start correctly, we send an error message to
 *	the serial monitor and flash the LED rapidly forever.
 */

	delay ( 100 );									// Without this, display doesn't start

	if( !Display.begin ( SSD1306_SWITCHCAPVCC, DISP_ADDR ))
	{
		Serial.println ( "SSD1306 allocation failed" );
		while ( true )
			BlinkLED ( 100, 5 );
	}

	attenuation = ATT_INIT;							// Set initial 'attenuation' value
	SetAttenuation ();								// Feed it to the attenuator module
	ShowAttenuation ();								// And display it

	digitalWrite ( LED, LED_ON );					// Turn on the alive indicator

	Serial.println ( "WA2FZW RF Attenuator - Version 2.0" );	// And announce on the serial monitor


/*
 *	Set up the interrupts for the rotary encoder. Some documents online would lead one to
 *	believe that the encoder can be handled without interrupts, but I couldn't make it work!
 *	If the encoder seems to work backwards, simply flip the pin number assignments above.
 */

	attachInterrupt ( digitalPinToInterrupt ( ENC_A ), ReadEncoder, CHANGE );
	attachInterrupt ( digitalPinToInterrupt ( ENC_B ), ReadEncoder, CHANGE );


/*
 *	The following is a test to see if the 'SetAttenuation' function works as
 *	expected and when measuring the frequency response for a particular module.
 *	Comment it out for normal operation.
 */

//	for ( attenuation = MIN_ATTENUATION; attenuation <= MAX_ATTENUATION;
//														attenuation += ENC_ADJUST )
//	{
//		SetAttenuation ();					// Set the module
//		delay ( 2000 );						// Time to observe the NanoVNA readings
//	}

//	while ( true ) {}						// Hang forever

}											// End of 'setup'


void loop ()
{
	if ( attChanged )							// If the encoder was moved
	{
		attChanged = false;						// Clear the changed flag
		SetAttenuation ();						// Tell the module
		ShowAttenuation ();						// Tell the operator
	}

	if ( !digitalRead ( ENC_SW ))				// If the encoder switch is active
	{
		while ( !digitalRead ( ENC_SW )) {}		// Wait for it to be released

		attenuation = ATT_INIT;					// Set the initial 'attemuation'
		attChanged = true;						// and indicate it changed
	}
}												// End of 'loop'


/*
 *	The 'ReadEncoder' function is called via the Arduino's interrupt mechanism, and
 *	is never invoked by any other means. Based on which way the encoder moved, we 
 *	either increment or decrement the 'attenuation' by 0.5dB per click.
 *
 *	But note, 'attenuation' is maintained as an interger value that is the desired
 *	'attenuation' times 10, so we increment or decrement  by '5' ('ENC_ADJUST'). We
 *	do this for two reasons; the Arduinos aren't great at floating point math and
 *	integer math is much faster.
 */

void ReadEncoder ()
{

/*
 *	Read the encoder. The Rotary object actually does the reading of the state of
 *	the encoder pins and determines which way it is moving. If the knob is being
 *	turned counter-clockwise, we decrement the 'attenuation'. If the knob is being
 *	turned clockwise, we increment 'attenuation.
 *
 *	If yours seems to work backwards, simply flip-flop the pin number definitions
 *	for 'ENC_A' and 'ENC_B' at the top of this file.
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

	attChanged = true;							// Signal that display & chip updates are needed
}


/*
 *	'SetAttenuation' tells the module what to do.
 *
 *	We have to break the 'attenuation' down into individual bits to be output to the
 *	'V1' through 'V6' pins.
 *
 *	Note that the 'ATT_OFFSET' does NOT come into play here; that only applies to the
 *	displayed value.
 */

void SetAttenuation ( void )
{
bool		v1, v2, v3, v4, v5, v6;				// Values to be fed to the module pins
uint16_t	localAtt;							// Local copy of 'attenuation'
float		realAtt;							// Floating point version for the serial monitor

	localAtt = attenuation;						// Make a copy of current 'attenuation' setting
	realAtt  = (float) attenuation / 10;		// For serial output only


/*
 *	A programming trick here! Folks are used to seeing logical expressions in things like
 *	'if' or 'while' statements, but note that the results of a logical expression can 
 *	also be assigned to a boolean variable! If the expression evaluates to 'true', the
 *	value of the variable will be '1'; if the expression evaluates to 'false' the value
 *	of the variable will be '0'!
 *
 *	Evaluating 'v1' is a bit tricky. Remember, the 'attenuation' is stored as an
 *	integer and the stored value is 10x the actual attenuation. To determine the
 *	value to be output to the 0.5 dB pin, we see if the remainder of dividing the
 *	'attenuation' by 10 is '5'.
 */

	v1 = (( localAtt % 10 ) == 5 );				// Isolate half dB step
	localAtt /= 10;								// and drop it from the value


/*
 *	For the rest, once we've divided the attenuation by 10, we just need to test the
 *	bits in the number.
 */

	v2 = localAtt & 1;							// 1.0 dB
	v3 = localAtt & 2;							// 2.0 dB
	v4 = localAtt & 4;							// 4.0 dB
	v5 = localAtt & 8;							// 8.0 dB
	v6 = localAtt & 16;						// 16.0 dB


/*
 *	Set the pins on the PE4301 module:
 */

	digitalWrite ( V1, v1);
	digitalWrite ( V2, v2);
	digitalWrite ( V3, v3);
	digitalWrite ( V4, v4);
	digitalWrite ( V5, v5);
	digitalWrite ( V6, v6);

#if	DEBUG

	Serial.print ( "PE4302 Attenuation = " );
	Serial.print ( realAtt );
	Serial.print ( ",  Binary = " );
	Serial.print ( v6 );
	Serial.print ( v5 );
	Serial.print ( v4 );
	Serial.print ( v3 );
	Serial.print ( v2 );
	Serial.println ( v1 );

#endif
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

#if	DEBUG

	Serial.print ( "Display Attenuation = " );
	Serial.println ( dispBuffer );

#endif

	Display.clearDisplay ();				

	Display.setTextSize ( 3 );					// Draw 3x scale text
	Display.setTextColor ( SSD1306_WHITE );		// Only color available!
	Display.setCursor ( 2, 10 );				// Top left corner of the text
	Display.print ( dispBuffer );				// Sets up the display's buffer
	Display.display ();							// Actually puts it on the screen
}


/*
 *	'BlinkLED' is used to indicat that some error occurred. So far the only error possible
 *	is a failure to initialize the display in which case the LED will blink forever.
 */

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
