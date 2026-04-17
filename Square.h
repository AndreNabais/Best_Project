#ifndef SQUARE_H_INCLUDED
#define SQUARE_H_INCLUDED

#include "Phasor.h"

class Square{
public:
  Square(int SR);
  
  void setFrequency(float f);
  void setGain(float g);
  float tick();
private:
  Phasor phasor;
  float gain;
  int samplingRate;
};

#endif  // SQUARE_H_INCLUDED