#ifndef CRONOMETRO_H  
#define CRONOMETRO_H

#include <Arduino.h> 

class Cronometro {
  private:
    unsigned long tiempoInicio; 
    unsigned long tiempoFin;    
    bool corriendo;             

  public:
    Cronometro();        
    void iniciar();       
    void detener();       
    unsigned long obtenerDuracion(); 
    void reiniciar();     
    bool estaCorriendo(); 
};

#endif 