
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "usb_device.h"


#include "pt.h"
#include "lepton.h"
#include "lepton_i2c.h"
#include "tmp007_i2c.h"
#include "usbd_uvc.h"
#include "usbd_uvc_if.h"


#include "usb_task.h"
#include "lepton_task.h"

#include "project_config.h"

extern volatile uint8_t uvc_stream_status;
extern USBD_UVC_VideoControlTypeDef videoCommitControl;

static int last_frame_count;
static lepton_buffer *last_buffer;

#ifdef USART_DEBUG
#define DEBUG_PRINTF(...) printf( __VA_ARGS__);
#else
#define DEBUG_PRINTF(...)
#endif

void HAL_RCC_CSSCallback(void) {
	DEBUG_PRINTF("Oh no! HAL_RCC_CSSCallback()\r\n");
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	DEBUG_PRINTF("Yay! HAL_GPIO_EXTI_Callback()\r\n");
}

#ifdef TMP007_OVERLAY 
#include "ugui.h"
UG_GUI gui; 
static void pixel_set(UG_S16 x , UG_S16 y ,UG_COLOR c )
{
#ifdef Y16
	last_buffer->lines[y].data.image_data[x] = c;
#else
	last_buffer->lines[y].data.image_data[x] = (rgb_t) { c, c, c };
#endif
}
#endif

static void draw_splash(int min, int max)
{
	int x_loc = 0;
	int y_loc = 0;

	//UG_PutChar(' ',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar(' ',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar(' ',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('P',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('U',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('R',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('E',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar(' ',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar(' ',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar(' ',x_loc,y_loc,max,min);

	x_loc=0;
	y_loc += 8;
	UG_PutChar('T',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('h',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('e',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('r',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('m',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('a',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('l',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar(' ',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('1',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar('0',x_loc,y_loc,max,min);

	x_loc=0;
	y_loc += 8;
	y_loc += 8;
	UG_PutChar('G',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('r',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('o',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('u',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('p',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('G',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('e',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('t',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('s',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar('0',x_loc,y_loc,max,min);

	x_loc=0;
	y_loc += 8;
	//UG_PutChar('G',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar('r',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar('o',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar('u',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar('p',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('.',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('c',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('o',x_loc,y_loc,max,min);
	x_loc+=8;
	UG_PutChar('m',x_loc,y_loc,max,min);
	x_loc+=8;
	//UG_PutChar('0',x_loc,y_loc,max,min);

}

#ifndef ENABLE_LEPTON_AGC
static void get_min_max(lepton_buffer *buffer, int * min, int * max)
{
	int i,j;
	int val;
	*min= 0xffff;
	*max= 0;
	for (j = 0; j < 60; j++)
	{
		for (i = 0; i < 80; i++)
		{
			val = buffer->lines[j].data.image_data[i];

			if( val > *max )
			{
				*max = val;
			}
			if( val < *min)
			{
				*min = val;
			}
		}
	}
}

static void scale_image_8bit(lepton_buffer *buffer, int min, int max)
{
	int i,j;
	int val;

	for (j = 0; j < 60; j++)
	{
		for (i = 0; i < 80; i++)
		{
			val = buffer->lines[j].data.image_data[i];
			val -= min;
			val = (( val * 255) / (max-min));

			buffer->lines[j].data.image_data[i] = val;
		}
	}
}
#endif

PT_THREAD( usb_task(struct pt *pt))
{
	static int temperature;
	static uint16_t count = 0, i;

	static uint8_t uvc_header[2] = { 2, 0 };
	static uint32_t uvc_xmit_row = 0, uvc_xmit_plane = 0;
	static uint8_t packet[VIDEO_PACKET_SIZE];

	PT_BEGIN(pt);

#ifdef TMP007_OVERLAY 
	UG_Init(&gui,pixel_set,80,60);
	UG_FontSelect(&FONT_8X8);     
#endif

	while (1)
	{
		 PT_WAIT_UNTIL(pt, get_lepton_buffer(NULL) != last_frame_count);
		 WHITE_LED_TOGGLE;
		 last_frame_count = get_lepton_buffer(&last_buffer);

#ifndef ENABLE_LEPTON_AGC
		get_min_max(last_buffer, &current_min, &current_max);
		scale_image_8bit(last_buffer, current_min, current_max);
#endif

#ifndef Y16
		// when we first start, byteswap the entire image
		for (i = 0; i < IMAGE_NUM_LINES; i++)
		{
			uint16_t* lineptr = (uint16_t*)last_buffer->lines[IMAGE_OFFSET_LINES + i].data.image_data;
			while (lineptr < (uint16_t*)&last_buffer->lines[IMAGE_OFFSET_LINES + i].data.image_data[FRAME_LINE_LENGTH])
			{
				uint8_t* bytes = (uint8_t*)lineptr;
				*lineptr++ = bytes[0] << 8 | bytes[1];
			}
		}
#endif

		if (((last_frame_count % 1800) > 0)   && ((last_frame_count % 1800) < 150)  )
		{
#ifdef SPLASHSCREEN_OVERLAY 
			draw_splash(255, 0);
#endif
		}
		else
		{

#ifdef TMP007_OVERLAY 
			temperature = get_last_mili_celisius()/1000;

			if(temperature < 0)
			{
				UG_PutChar('-',0,51,10000,0);
				temperature = -temperature;
			}
			else if(((temperature/100)%10) != 0 ) 
			{
				UG_PutChar((temperature/100)%10 + '0',0,51,255,0);
			}

			UG_PutChar((temperature/10)%10 + '0',8,51,255,0);
			UG_PutChar(temperature%10 + '0',16,51,255,0);
			UG_PutChar(248,24,51,255,0);
			UG_PutChar('C',32,51,255,0);
#endif
		}



    // perform stream initialization
    if (uvc_stream_status == 1)
    {
      DEBUG_PRINTF("Starting UVC stream...\r\n");

      uvc_header[0] = 2;
      uvc_header[1] = 0;
      UVC_Transmit_FS(uvc_header, 2);

      uvc_stream_status = 2;
      uvc_xmit_row = 0;
      uvc_xmit_plane = 0;
    }

    // put image on stream as long as stream is open
    while (uvc_stream_status == 2)
    {
      count = 0;

      packet[count++] = uvc_header[0];
      packet[count++] = uvc_header[1];

      switch (videoCommitControl.bFormatIndex[0])
      {
        // HACK: NV12 is semi-planar but YU12/I420 is planar. Deal with it when we have actual color.
        default:
        case VS_FMT_INDEX(NV12):
        case VS_FMT_INDEX(YU12):
        {
          // printf("Writing format 1, plane %d...\r\n", uvc_xmit_plane);

          switch (uvc_xmit_plane)
          {
            default:
            case 0: // Y
            {
              // while (uvc_xmit_row < 60 && count < VALDB(videoCommitControl.dwMaxPayloadTransferSize))
              while (uvc_xmit_row < IMAGE_NUM_LINES && count < VIDEO_PACKET_SIZE)
              {
                for (i = 0; i < FRAME_LINE_LENGTH; i++)
                {
#ifdef Y16
                  uint16_t val = last_buffer->lines[IMAGE_OFFSET_LINES + uvc_xmit_row].data.image_data[i];
#else
                  uint8_t val = last_buffer->lines[IMAGE_OFFSET_LINES + uvc_xmit_row].data.image_data[i].g;
#endif
                  // AGC is on so just use lower 8 bits
                  packet[count++] = (uint8_t)val;
                }

                uvc_xmit_row++;
              }

              if (uvc_xmit_row == IMAGE_NUM_LINES)
              {
                uvc_xmit_plane = 1;
                uvc_xmit_row = 0;
              }

              break;
            }
            case 1: // VU
            case 2:
            {
              if (videoCommitControl.bFormatIndex[0] == VS_FMT_INDEX(NV12))
              {
                while (uvc_xmit_row < (IMAGE_NUM_LINES/2) && count < VIDEO_PACKET_SIZE)
                {
                  for (i = 0; i < FRAME_LINE_LENGTH && uvc_xmit_row < (IMAGE_NUM_LINES/2) && count < VIDEO_PACKET_SIZE; i++)
                    packet[count++] = 128;

                  uvc_xmit_row++;
                }

                if (uvc_xmit_row == 30)
                  packet[1] |= 0x2; // Flag end of frame
              }
              else
              {
                while (uvc_xmit_row < (IMAGE_NUM_LINES/2) && count < VIDEO_PACKET_SIZE)
                {
                  for (i = 0; i < (FRAME_LINE_LENGTH/2) && uvc_xmit_row < (IMAGE_NUM_LINES/2) && count < VIDEO_PACKET_SIZE; i++)
                    packet[count++] = 128;

                  uvc_xmit_row++;
                }

                // image is done
                if (uvc_xmit_row == (IMAGE_NUM_LINES/2))
                {
                  if (uvc_xmit_plane == 1)
                  {
                    uvc_xmit_plane = 2;
                    uvc_xmit_row = 0;
                  }
                  else
                  {
                    packet[1] |= 0x2; // Flag end of frame
                  }
                }
              }
              break;
            }
          }
  
          break;
        }
        case VS_FMT_INDEX(GREY):
        {
          // while (uvc_xmit_row < 60 && count < VALDB(videoCommitControl.dwMaxPayloadTransferSize))
          while (uvc_xmit_row < IMAGE_NUM_LINES && count < VIDEO_PACKET_SIZE)
          {
            for (i = 0; i < FRAME_LINE_LENGTH; i++)
            {
#ifdef Y16
              uint16_t val = last_buffer->lines[IMAGE_OFFSET_LINES + uvc_xmit_row].data.image_data[i];
#else
              uint8_t val = last_buffer->lines[IMAGE_OFFSET_LINES + uvc_xmit_row].data.image_data[i].g;
#endif
              // AGC is on, so just use lower 8 bits
              packet[count++] = (uint8_t)val;
            }

            uvc_xmit_row++;
          }

          // image is done
          if (uvc_xmit_row == IMAGE_NUM_LINES)
          {
            packet[1] |= 0x2; // Flag end of frame
          }

          break;
        }
        case VS_FMT_INDEX(Y16):
        {
          // while (uvc_xmit_row < 60 && count < VALDB(videoCommitControl.dwMaxPayloadTransferSize))
          while (uvc_xmit_row < IMAGE_NUM_LINES && count < VIDEO_PACKET_SIZE)
          {
            for (i = 0; i < FRAME_LINE_LENGTH; i++)
            {
#ifdef Y16
              uint16_t val = last_buffer->lines[IMAGE_OFFSET_LINES + uvc_xmit_row].data.image_data[i];
#else
              uint8_t val = last_buffer->lines[IMAGE_OFFSET_LINES + uvc_xmit_row].data.image_data[i].g;
#endif
              packet[count++] = (uint8_t)((val >> 0) & 0xFF);
              packet[count++] = (uint8_t)((val >> 8) & 0xFF);
            }

            uvc_xmit_row++;
          }

          // image is done
          if (uvc_xmit_row == IMAGE_NUM_LINES)
          {
            packet[1] |= 0x2; // Flag end of frame
          }

          break;
        }
        case VS_FMT_INDEX(YUYV):
        {
#ifdef Y16
          // while (uvc_xmit_row < 60 && count < VALDB(videoCommitControl.dwMaxPayloadTransferSize))
          while (uvc_xmit_row < IMAGE_NUM_LINES && count < VIDEO_PACKET_SIZE)
          {
            for (i = 0; i < FRAME_LINE_LENGTH; i++)
            {
              uint16_t val = last_buffer->lines[IMAGE_OFFSET_LINES + uvc_xmit_row].data.image_data[i];
              // AGC is on so just use lower 8 bits
              packet[count++] = (uint8_t)val;
              packet[count++] = 128;
            }

            uvc_xmit_row++;
          }
#else
          while (uvc_xmit_row < IMAGE_NUM_LINES && count < VIDEO_PACKET_SIZE)
          {
            for (i = 0; i < FRAME_LINE_LENGTH; i++)
            {
              uint8_t UV;
              rgb_t val = last_buffer->lines[IMAGE_OFFSET_LINES + uvc_xmit_row].data.image_data[i];

              uint8_t Y1 =  (0.257f * (float)val.r) + (0.504f * (float)val.g) + (0.098f * (float)val.b);

              if ((i % 2) == 0)
                UV = -(0.148f * (float)val.r) - (0.291f * (float)val.g) + (0.439f * (float)val.b) + 128;
              else
                UV =  (0.439f * (float)val.r) - (0.368f * (float)val.g) - (0.071f * (float)val.b) + 128;

              packet[count++] = Y1;
              packet[count++] = UV;
            }

            uvc_xmit_row++;
          }
#endif

          // image is done
          if (uvc_xmit_row == IMAGE_NUM_LINES)
          {
            packet[1] |= 0x2; // Flag end of frame
          }

          break;
        }
      }

      // printf("UVC_Transmit_FS(): packet=%p, count=%d\r\n", packet, count);
      // fflush(stdout);

      while (UVC_Transmit_FS(packet, count) == USBD_BUSY && uvc_stream_status == 2)
        ;

      if (packet[1] & 0x2)
      {
        uvc_header[1] ^= 1; // toggle bit 0 for next new frame
        uvc_xmit_row = 0;
        uvc_xmit_plane = 0;
        // DEBUG_PRINTF("Frame complete\r\n");
        break;
      }
      PT_YIELD(pt);
    }
    PT_YIELD(pt);
	}
	PT_END(pt);
}
