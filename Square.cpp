#include <cmath>

#include "Square.h"

Square::Square(int SR) : 
phasor(SR),
gain(1.0),
samplingRate(SR){}

void Square::setFrequency(float f){
  phasor.setFrequency(f);
}
    
void Square::setGain(float g){
  gain = g;
}
    
float Square::tick(){
  float phase = phasor.tick();
  // If phase is less than 0.5, output 1.0. Otherwise, output -1.0.
  if (phase < 0.5) {
    return gain; // Output 1.0 multiplied by gain
  } else {
    return -gain; // Output -1.0 multiplied by gain
  }

}
