#ifndef _SDRLIB_H_
#define _SDRLIB_H_
/**
 * This is intended as a small and simple
 * SDR library with as low code complexity, 
 * and as few dependencies as possible.  
 * 
 * The goal is to make code readability, maintainability
 * and portability as high as possible.   
 *
 * Authors:
 *   Bob Jamison
 *
 * Copyright (C) 2013 Bob Jamison
 * 
 *  This file is part of the SdrLib library.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <pthread.h>


#ifdef __cplusplus
extern "C" {
#else
#include <complex.h>
typedef void ComplexOutputFunc(float complex *data, int size, void *ctx);
#endif

typedef void ByteOutputFunc(unsigned char *data, int size, void *ctx);
typedef void UintOutputFunc(unsigned int *ps, int size, void *ctx);
typedef void FloatOutputFunc(float *data, int size, void *ctx);



#define SDR_MAX_DEVICES 30



/**
 * Forward declarations, hidden from clients
 */
typedef struct Audio       Audio; 
typedef struct Biquad      Biquad;
typedef struct Codec       Codec; 
typedef struct Ddc         Ddc; 
typedef struct Decimator   Decimator; 
typedef struct Demodulator Demodulator; 
typedef struct Device      Device; 
typedef struct Fir         Fir; 
typedef struct Fft         Fft; 
typedef struct Resampler   Resampler;
typedef struct Queue       Queue; 
typedef struct Vfo         Vfo; 

typedef struct SdrLib      SdrLib;

typedef enum
{
    MODE_NULL=0,
    MODE_AM,
    MODE_FM,
    MODE_LSB,
    MODE_USB
} Mode;



/**
 * Create a new SdrLib instance.
 * @return a new SdrLib instance
 */  
SdrLib *sdrCreate(void *context, UintOutputFunc *psFunc, ByteOutputFunc *codecFunc);


/**
 * Delete an SdrLib instance, stopping
 * any processing and freeing any resources.
 * @param sdrlib an SDRLib instance.
 */   
int sdrDelete(SdrLib *sdrlib);



/**
 * Start sdrlib processing
 * @param sdrlib an SDRLib instance.
 */   
int sdrStart(SdrLib *sdrlib);



/**
 * Stop sdrlib processing
 * @param sdrlib an SDRLib instance.
 */   
int sdrStop(SdrLib *sdrlib);

/**
 * Get the current center frequency
 * @param sdrlib an SDRLib instance.
 */   
double sdrGetCenterFrequency(SdrLib *sdrlib);



/**
 * Stop sdrlib processing
 * @param sdrlib an SDRLib instance.
 */   
int sdrSetCenterFrequency(SdrLib *sdrlib, double freq);

/**
 */   
void sdrSetDdcFreqs(SdrLib *sdr, float vfo, float pbLo, float pbHi);

/**
 */   
void sdrSetVfo(SdrLib *sdr, float vfo);

/**
 */   
float sdrGetVfo(SdrLib *sdr);

/**
 */   
void sdrSetPbLo(SdrLib *sdr, float pbLo);

/**
 */   
float sdrGetPbLo(SdrLib *sdr);

/**
 */   
void sdrSetPbHi(SdrLib *sdr, float pbHi);

/**
 */   
float sdrGetPbHi(SdrLib *sdr);

/**
 * Get the current sample rate, in samples/sec
 * @param sdrlib an SDRLib instance.
 */   
float sdrGetSampleRate(SdrLib *sdrlib);



/**
 * Set the sample rate
 * @param sdrlib an SDRLib instance.
 */   
int sdrSetSampleRate(SdrLib *sdrlib, float rate);

/**
 * Get gain 0-1
 * @param sdrlib an SDRLib instance.
 */   
float sdrGetRfGain(SdrLib *sdrlib);



/**
 * Set gain 0-1
 * @param sdrlib an SDRLib instance.
 */   
int sdrSetRfGain(SdrLib *sdrlib, float gain);


/**
 * Get gain 0-1
 * @param sdrlib an SDRLib instance.
 */   
float sdrGetAfGain(SdrLib *sdrlib);



/**
 * Set gain 0-1
 * @param sdrlib an SDRLib instance.
 */   
int sdrSetAfGain(SdrLib *sdrlib, float gain);


/**
 * Get current demodulation Mode
 * @param sdrlib an SDRLib instance.
 */   
int sdrGetMode(SdrLib *sdrlib);



/**
 * Set current demodulation Mode
 * @param sdrlib an SDRLib instance.
 */   
int sdrSetMode(SdrLib *sdrlib, Mode mode);


/**
 * Determine if we want speaker output
 * @param sdrlib an SDRLib instance.
 * @param enabled 0 to disable, !=0 enabled
 */   
void sdrEnableAudio(SdrLib *sdr, int enabled);


#ifdef __cplusplus
}
#endif



#endif /* _SDRLIB_H_ */

