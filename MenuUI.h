/* 
   This file is part of MenuUI, a flexible menu driven user interface 
   for Arduino.
   Copyright (C) 2016-2017 Daniel Pena <trifling.github@gmail.com>

   MenuUI is free software: you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation, either version 3 of the License, or (at your option) any later
   version.

   MenuUI is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along with
   closest. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MENU_H
#define MENU_H

#include <EEPROM.h>
#include <Arduino.h>

#ifndef MENU_LINE_SIZE
#define MENU_LINE_SIZE 16
#endif

#ifndef MENU_LCDOFF_MSECS
#define MENU_LCDOFF_MSECS 20000
#endif

#ifndef MENU_BLINK_MSECS 
#define MENU_BLINK_MSECS 500
#endif

#ifndef MENU_VALUE_MSECS 
#define MENU_VALUE_MSECS 500
#endif

#define MENU_DETAILED (MENU_OPTION|MENU_VALUE)
#define MENU_UNDEF UINT8_MAX

/*
 *
 * OPTIONS
 *
 */

class OptionBase {
public:
   virtual void startChange()=0;
   virtual void endChange()=0;
   virtual void asString( char *s, uint8_t n )=0;
   virtual void change(int8_t delta)=0;
};

template<typename T>
class OptionCommon : public OptionBase {
public:
   void startChange() {
      m_tmp = m_val;
      m_changing = 1;
   }
   void endChange() {
      m_val = m_tmp;
      m_changing = 0;
   }
   int save( int addr ) {
      const uint8_t* ptr = (const uint8_t*)&m_val;
      uint8_t i;
      for( i = 0; i < sizeof(m_val); i++ )
         EEPROM.update( addr++, *ptr++ );
      return i;  
   }
   int read( int addr ) {
      uint8_t* ptr = (uint8_t*)&m_val;
      uint8_t i;
      for( i = 0; i < sizeof(m_val); i++ )
         *ptr++ = EEPROM.read(addr++);
      return i;
   }
   void asString( char *s, uint8_t n ) {
      if( m_changing )
         asString( m_tmp, s, n );
      else
         asString( m_val, s, n );
   }
   virtual void asString( T val, char *s, uint8_t n );
   virtual void change(int8_t delta)=0;
   T value() {
      return m_val;
   }
protected:
   T m_val, m_tmp;
   uint8_t m_changing;
};

template<typename T>
class NumericOption : public OptionCommon<T> {
public:
   NumericOption( T val, T min, T max, T step, const char *unit = NULL ) {
      OptionCommon<T>::m_val  = val;
      m_min  = min;
      m_max  = max;
      m_step = step;
      m_unit = unit;
   }
   void change( int8_t delta ) {
      if( delta > 0 ) {
         OptionCommon<T>::m_tmp += m_step;
         if( OptionCommon<T>::m_tmp > m_max )
            OptionCommon<T>::m_tmp = m_max;
      } else if( delta < 0 ) {
         OptionCommon<T>::m_tmp -= m_step;
         if( OptionCommon<T>::m_tmp < m_min )
            OptionCommon<T>::m_tmp = m_min;
      }
   }
   void asString( T val, char *s, uint8_t n );
protected:
   T m_step, m_max, m_min;
   const char *m_unit;
};
typedef NumericOption<int>   IntOption;
typedef NumericOption<float> FloatOption;

class BoolOption : public OptionCommon<bool> {
public:
   BoolOption( bool val, bool on_off = false ) {
      OptionCommon<bool>::m_val  = val;
      m_on_off = on_off;
   }
   void change( int8_t delta ) {
      if( OptionCommon<bool>::m_tmp )
         OptionCommon<bool>::m_tmp = false;
      else 
         OptionCommon<bool>::m_tmp = true;
   }
   void asString( bool val, char *s, uint8_t n ) {
      if( val ) {
         if( m_on_off )
            strcpy( s, "ON" );
         else
            strcpy( s, "TRUE" );
      } else {
         if( m_on_off )
            strcpy( s, "OFF" );
         else
            strcpy( s, "FALSE" );
      }
   }
protected:
   bool m_on_off;
};



/*
 *
 * MENU ITEMS
 *
 */
typedef struct {
   uint8_t id;
   uint8_t sub_id; 
   uint8_t type;   
   const char *caption;
   OptionBase *option;
   uint8_t first;  
   uint8_t parent; 
   uint8_t count;
} MenuItem;



enum MenuItemType { 
   MENU_START   = 1,
   MENU_SUBMENU = 2,
   MENU_OPTION  = 4,
   MENU_VALUE   = 8,
   MENU_BACK    = 16,
   MENU_EXIT    = 32,
   MENU_END     = 64 
};

enum {
   MENU_LCD_ON = 1,
   MENU_LCD_OFF = 2,
   MENU_LCD_PRINT_LINE1 = 4, 
   MENU_LCD_PRINT_LINE2 = 8,
};

typedef void (*lcd_fun)( uint8_t mode, const char *line );
typedef void (*val_fun)( uint8_t   id, char *line );

/*
 *
 * MENU BASE CLASS 
 *
 */
class MenuUI {

public:

   MenuUI();
   
   void setup( MenuItem *items, uint8_t line_size, lcd_fun lfun, val_fun vfun );
   void update( unsigned long current_time, int8_t delta, int8_t button  );
   void uptime( char *out );

   void setBlinkTime( uint16_t msecs );
   void setLCDOffTime( uint16_t msecs );
   void setValueUpdateTime( uint16_t msecs );

   ~MenuUI();

private:
   MenuItem *m_items;
   lcd_fun m_lfun;
   val_fun m_vfun;
   uint8_t m_lcdon;
   uint8_t m_detail;
   uint8_t m_current;
   unsigned long m_last_time;
   unsigned long m_blink_time;
   unsigned long m_value_time;
   uint8_t m_blink_status;
   char *m_line1;
   char *m_line2;
   unsigned long m_highmillis;
   uint16_t m_rollovers;
   uint16_t m_blink_msecs;
   uint16_t m_lcdoff_msecs;
   uint16_t m_value_msecs;
   uint8_t m_line_size;
};

#endif

