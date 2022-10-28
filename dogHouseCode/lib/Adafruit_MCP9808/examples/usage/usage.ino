// Example usage for Temp2Library library by Adafruit.

#include "Temp2Library.h"

// Initialize objects from the lib
Temp2Library temp2Library;

void setup() {
    // Call functions on initialized library objects that require hardware
    temp2Library.begin();
}

void loop() {
    // Use the library's initialized objects and functions
    temp2Library.process();
}
