/**
 * @file crono.h
 * @brief Clase para medición de tiempos de ejecución
 * @author Elathia
 * @date 2025
 * @version 2.0
 * 
 * @details Cronómetro simple para medir duraciones en milisegundos.
 * Útil para diagnóstico y optimización de rendimiento del sistema.
 */

#ifndef CRONOMETRO_H  
#define CRONOMETRO_H

#include <Arduino.h>

/**
 * @class Cronometro
 * @brief Clase para medición de intervalos de tiempo
 */
class Cronometro {
  private:
    unsigned long tiempoInicio;
    unsigned long tiempoFin;
    bool corriendo;

  public:
    /**
     * @brief Constructor por defecto
     */
    Cronometro();
    
    /**
     * @brief Inicia el cronómetro
     */
    void iniciar();
    
    /**
     * @brief Detiene el cronómetro
     */
    void detener();
    
    /**
     * @brief Obtiene la duración medida
     * @return Tiempo transcurrido en milisegundos
     */
    unsigned long obtenerDuracion();
    
    /**
     * @brief Reinicia el cronómetro a cero
     */
    void reiniciar();
    
    /**
     * @brief Verifica si el cronómetro está corriendo
     * @return true si está activo, false si está detenido
     */
    bool estaCorriendo();
};

#endif 